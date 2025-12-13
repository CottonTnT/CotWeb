#include <cerrno>
#include <expected>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

#include "net/Acceptor.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/Socketsops.h"
#include "net/Timestamp.h"

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : owner_loop_ {loop}
    , accept_socket_ {Sock::createNonblockingOrDie(listenAddr.getFamily())}
    , listen_channel_ {loop, accept_socket_.GetFd()} // 为监听套接字创建 Channel
    , listenning_ {false}
    , idle_fd_ {::open("/dev/null", O_RDONLY | O_CLOEXEC)} // 打开一个文件描述符，防止文件描述符耗尽
{
    accept_socket_.setReuseAddr(true);
    accept_socket_.setReusePort(reuseport);
    accept_socket_.bindAddress(listenAddr);
    // TcpServer::start() => Acceptor.listen() 如果有新用户连接 要执行一个回调(accept => connfd => 打包成Channel => 唤醒subloop)
    // baseloop监听到有事件发生 => accept_channel_(listenfd) => 执行该回调函数

    listen_channel_.setReadCallback([acceptor = this](Timestamp) {
        acceptor->socketChannelReadCB_();
    });
}

Acceptor::~Acceptor()
{
    listen_channel_.unregisterAllEvent(); // 把从Poller中感兴趣的事件删除掉
    listen_channel_.remove(); // 调用EventLoop->removeChannel => Poller->removeChannel 把Poller的ChannelMap对应的部分删除
}

void Acceptor::listenInOwnerThread()
{
    owner_loop_->assertInOwnerThread();
    // 1. listen
    listenning_ = true;
    accept_socket_.listen();
    // 2. register read event in poller
    listen_channel_.enableReading();
}

// listenfd有事件发生了，就是有新用户连接了
void Acceptor::socketChannelReadCB_()
{
    auto peer_addr = InetAddress {};

    auto success_do = [this, &peer_addr](int connfd)
        -> std::expected<void, std::error_code> {
        // ✅ 成功路径：当 Accept 返回一个有效的文件描述符 (connfd) 时，这个 lambda 会被执行
        if (new_conn_callback_)
        {
            new_conn_callback_(connfd, peer_addr);
        }
        else
        {
            defautlNewConnectionCallback_(connfd);
        }
        // 返回一个空的成功状态，表示成功路径已处理完毕
        return {};
    };
    auto error_do = [this](const std::error_code& ec)
        -> std::expected<void, std::error_code> {
        if (ec == std::errc::too_many_files_open) // 使用 std::errc 枚举更类型安全
        {
            // todo:log
            //  执行与原来完全相同的 EMFILE 错误恢复逻辑
            ::close(idle_fd_);
            idle_fd_ = ::accept4(accept_socket_.GetFd(), nullptr, nullptr, SOCK_CLOEXEC);
            ::close(idle_fd_);
            idle_fd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
        // 对于其他错误，我们什么也不做，和原逻辑一致

        // 返回一个空的成功状态，表示错误已被“处理”（即使处理方式是忽略）
        return {};
    };
    // 开始链式调用
    std::ignore = accept_socket_.accept(peer_addr)
                      .and_then(success_do)
                      .or_else(error_do);

    // todo: loop until no more
    // auto connfd = accept_socket_.Accept(&peer_addr);
    // if (connfd >= 0)
    // {
    //     if (new_conn_callback_)
    //     {
    //         new_conn_callback_(connfd, peer_addr); // 轮询找到subLoop 唤醒并分发当前的新客户端的Channel
    //     }
    //     else
    //     {
    //         ::close(connfd);
    //     }
    // }
    // else
    // {
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.
    //     if (errno == EMFILE)
    //     {
    // LOG_ERROR("%s:%s:%d sockfd reached limit\n", __FILE__, __FUNCTION__, __LINE__);
    //         ::close(idle_fd_);
    //         idle_fd_ = accept4(accept_socket_.GetFd(), nullptr, nullptr, SOCK_CLOEXEC);
    //         ::close(idle_fd_);
    //         idle_fd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    //     }
    // why do this?
    // Linux 下每个进程能打开的文件描述符数是有限的（ulimit -n 控制）。
    // 如果 进程 fd 用光了，再调用 accept() 时会失败，返回 EMFILE（"Too many open files"）。这时候会有个严重的问题：内核已经完成三次握手，把连接放到了已完成队列。但是 accept() 拒绝了，连接卡在那儿，客户端迟迟收不到拒绝信号，最终可能会超时。
    // 很多高性能网络库（如 nginx、muduo）都会用 idleFd_ 技巧 来优雅处理 EMFILE：
    // 平时持有一个无用的 idleFd_（比如 /dev/null）。占着一个 fd 名额。当 accept() 返回 EMFILE：
    // 先 ::close(idleFd_)，释放出一个 fd 名额。
    // 再 ::accept() 一次，立刻拿到这个新连接的 fd。
    // 然后对这个新连接 ::close(fd)（相当于告诉客户端：连接被拒绝）。
    // 最后重新打开 /dev/null 填回 idleFd_，继续待命
    // }
}