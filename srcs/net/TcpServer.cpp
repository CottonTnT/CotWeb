#include <latch>
#include <memory>

#include "net/TcpServer.h"
#include "net/Callbacks.h"
#include "net/TcpConnection.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"

static auto log = GET_ROOT_LOGGER();

static auto requiresNonNull(EventLoop* loop)
    -> EventLoop*
{
    if (loop == nullptr)
    {
        // LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     std::string nameArg,
                     Option option)
    : base_loop_ {requiresNonNull(loop)}
    , ipport_repr_ {listenAddr.toIpPortRepr()}
    , name_ {std::move(nameArg)}
    , acceptor_ {new Acceptor {loop, listenAddr, option == kReusePort}}
    , threadpool_ {new EventLoopThreadPool(loop, name_)}
    , conn_established_callback_(defaultConnectionCallback)
    , conn_close_callback_(defaultConnectionCallback)
    , msg_callback_ {defaultMessageCallback}
    , started_ {0}
    , next_conn_id_ {1}
{
    // there tcpserver* was captured by value, cause acceptor is a member of tcpserver
    // 当有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生，执行handleRead()调用TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback([this](int sockfd, const InetAddress& peerAddr) {
        this->initNewConnInOwnerThread_(sockfd, peerAddr);
    });
}

TcpServer::~TcpServer()
{
    base_loop_->assertInOwnerThread();
    LOG_DEBUG_FMT(log, "TcpServer::~TcpServer[{}] destructing", name_);

    for (auto& item : connections_)
    {
        auto conn = item.second;
        // 把原始的智能指针复位 让栈空间的TcpConnectionPtr conn指向该对象 当conn出了其作用域 即可释放智能指针指向的对象
        // 销毁连接
        item.second.reset();
        conn->getLoop()->runTask([guard_conn = conn] {
            if (guard_conn->state_ != TcpConnection::Disconnected)
            {
                guard_conn->socketChannelCloseCB_();
            }
        });
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadpool_->setThreadNum(numThreads);
}

// 开启服务器监听, 但未开启事件循环
void TcpServer::start()
{
    if (started_++ == 0) // 防止一个TcpServer对象被start多次
    {
        // 1. 启动底层的loop线程池
        threadpool_->start();

        // 2.将 Acceptor::listen 任务提交到主 EventLoop 执行以启动监听
        base_loop_->runTask([this]() -> void {
            this->acceptor_->listenInOwnerThread();
        });
    }
}

/**
 * @brief  有一个新用户连接，acceptor会执行这个回调操作，负责将mainLoop接收到的请求连接(acceptChannel_会有读事件发生)通过回调轮询分发给subLoop去处理
 */
void TcpServer::initNewConnInOwnerThread_(int sockfd, const InetAddress& peerAddr)
{
    base_loop_->assertInOwnerThread();

    // 1. 轮询算法 选择一个subLoop 来管理 connfd 对应的 channel
    auto* choosen_io_loop = threadpool_->getNextLoop();

    // 2. 创建并初始化新连接
    // 2.1 build the new connection name
    auto buf        = std::array<char, 256> {};
    auto stop_pos   = std::format_to_n(buf.begin(), buf.size(), "-{}#{}", ipport_repr_, next_conn_id_);
    stop_pos.out[0] = '\0';

    // 这里没有设置为原子类是因为其只在mainloop中执行 不涉及线程安全问题
    ++next_conn_id_;
    auto new_conn_name = name_ + buf.data();

    LOG_INFO_FMT(log,"TcpServer::newConnection [{}] - new connection [{}] from {}",
     name_, new_conn_name, peerAddr.toIpPortRepr());

    // 2.2 通过sockfd获取其绑定的本机的ip地址和端口信息
    auto local_addr = InetAddress::GetLocalInetAddress(sockfd);

    // build the new connection
    auto new_conn = std::make_shared<TcpConnection>(choosen_io_loop,
                                                    new_conn_name,
                                                    sockfd,
                                                    local_addr,
                                                    peerAddr);
    // 3. 保存连接, 并设置新连接的事件回调
    connections_[new_conn_name] = new_conn;

    // new_conn->SetCloseCallback(
    //     std::bind(&TcpServer::RemoveConnection_, this, std::placeholders::_1));

    new_conn->setConnetionCallback(conn_established_callback_);

    // avoid TcpServer destructed before TcpConnection
    new_conn->setCloseCallback([user_close_cb = conn_close_callback_, a = std::weak_ptr<TcpServer> {shared_from_this()}](const TcpConnectionPtr& conn) {
        user_close_cb(conn);
        if (auto guard = a.lock(); guard != nullptr)
        {
            // 2.执行TcpServer内部的连接移除操作
            // guard->conn_close_callback_(conn);
            guard->removeConnection_(conn);
            return;
        }
    });

    new_conn->setMessageCallback(msg_callback_);
    new_conn->setWriteCompleteCallback(write_complete_callback_);

    // 让subloop执行新连接的建立 回调TcpConnection::connectEstablished

    // 将连接建立的后续操作交给 ioLoop 执行
    auto connnect_established_task = [new_conn]() -> void {
        new_conn->postConnectionCreate_();
    };
    choosen_io_loop->runTask(connnect_established_task);
}

void TcpServer::removeConnection_(const TcpConnectionPtr& conn)
{
    // here capture tcpserver by this pointer is ok, cause tcpserver must be alive when removeConnection_ is called
    // otherwise, removeConnection will not be called in conn->closeCallback_
    base_loop_->runTask([this, conn]() {
        this->removeConnectionInOwnerThread_(conn);
    });
}

void TcpServer::removeConnectionInOwnerThread_(const TcpConnectionPtr& conn)
{
    base_loop_->assertInOwnerThread();

    LOG_INFO_FMT(log, "TcpServer::removeConnectionInLoop [{}] - connection {}", name_, conn->getName());
    connections_.erase(conn->getName());
    auto* io_loop = conn->getLoop();
    // make sure tcpconn destruct in owner loop thread, 单一职责，线程安全
    io_loop->queueTask([tcpconn = conn] { tcpconn->destructConnectionInOnwerLoop_(); });
}