#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
#include <array>
#include <utility>

#include "net/Buffer.h"

std::string Buffer::c_CRLF = "\r\n";

Buffer::Buffer(Buffer&& rhs) noexcept
    : buffer_(std::move(rhs.buffer_))
    , reader_idx_ {std::exchange(rhs.reader_idx_, kCheapPrepend)}
    , writer_idx_ {std::exchange(rhs.writer_idx_, kCheapPrepend)}
{
}

auto Buffer::operator=(Buffer&& rhs) noexcept
    -> Buffer&
{
    if (this == &rhs)
        return *this;
    // copy and swap idiom
    using std::swap;
    auto tmp_buf = std::move(rhs);
    swap(*this, tmp_buf);
    return *this;
}

auto Buffer::getReadableBytesCount() const
    -> std::size_t { return writer_idx_ - reader_idx_; }
auto Buffer::getWritableBytesCount() const
    -> std::size_t { return buffer_.size() - writer_idx_; }
auto Buffer::getPrependableBytesCount() const
    -> std::size_t { return reader_idx_; }

// 返回缓冲区中可读数据的起始地址
auto Buffer::getReadPos_() const
    -> const unsigned char*
{
    return (const_cast<Buffer*>(this))->getReadPos_();
}

auto Buffer::getReadPos_()
    -> unsigned char*
{
    return begin_() + reader_idx_;
}

auto Buffer::getWritePos_() const
    -> const unsigned char*
{
    return (const_cast<Buffer*>(this))->getWritePos_();
}

auto Buffer::getWritePos_()
    -> unsigned char*
{
    return begin_() + writer_idx_;
}

[[nodiscard]] auto Buffer::findCrLf() const
    -> const unsigned char*
{
    // FIXME: replace with memmem()?
    const auto* crlf_pos = std::search(getReadPos_(), getWritePos_(), c_CRLF.begin(), c_CRLF.end());
    return crlf_pos == getWritePos_() ? nullptr : crlf_pos;
}

auto Buffer::findCrLf(const unsigned char* stPos) const
    -> const unsigned char*
{
    assert(getReadPos_() <= stPos);
    assert(stPos <= getWritePos_());
    const auto* crlf = std::search(stPos, getWritePos_(), c_CRLF.begin(), c_CRLF.end());
    return crlf == getWritePos_() ? nullptr : crlf;
}

[[nodiscard]] auto Buffer::findEol() const
    -> const char*
{
    const auto* eol = memchr(getReadPos_(), '\n', getReadableBytesCount());
    return static_cast<const char*>(eol);
}

auto Buffer::findEol(const unsigned char* stPos) const
    -> const char*
{
    assert(getReadPos_() <= stPos);
    assert(stPos <= getWritePos_());
    const auto* eol = memchr(stPos, '\n', getWritePos_() - stPos);
    return static_cast<const char*>(eol);
}

[[nodiscard]] auto Buffer::toReadableSpan()
    -> std::span<unsigned char>
{
    return std::span<unsigned char> {getReadPos_(), getReadableBytesCount()};
}

[[nodiscard]] auto Buffer::toReadableSV()
    -> std::string_view
{
    return std::string_view {reinterpret_cast<char*>(getReadPos_()), getReadableBytesCount()};
}

void Buffer::swap(Buffer& rhs)
{
    buffer_.swap(rhs.buffer_);
    std::swap(reader_idx_, rhs.reader_idx_);
    std::swap(writer_idx_, rhs.writer_idx_);
}
/**
 * muduo 通常工作在 LT (电平触发) 模式下，为了避免因数据未读完而导致的事件重复触发，需要一次性将 socket 缓冲区* 的数据尽可能读完。Buffer::readFd 正是为此设计的，它通过 readv (分散读) 系统调用和栈上临时缓冲区 extrabuf，
 * 巧妙地解决了这个问题，同时优化了性能。Buffer 可以以较小的初始大小启动，readFd 利用栈上临时空间来处理突发的大
 * 量数据，只有在确认需要时才真正扩容 Buffer
 **/
auto Buffer::readFd(int fd, int* saveErrno)
    -> ssize_t
{
    // 栈额外空间，用于从套接字往出读时，当buffer_暂时不够用时暂存数据，待buffer_重新分配足够空间后，在把数据交换给buffer_。
    auto extrabuf = std::array<char, 65536> {}; // 栈上内存空间 65536/1024 = 64KB
    extrabuf.fill('\0');

    /*
    struct iovec {
        ptr_t iov_base; // iov_base指向的缓冲区存放的是readv所接收的数据或是writev将要发送的数据
        size_t iov_len; // iov_len在各种情况下分别确定了接收的最大长度以及实际写入的长度
    };
    */

    // 使用iovec分配两个连续的缓冲区
    struct iovec vec[2];
    const auto writable = getWritableBytesCount(); // 这是Buffer底层缓冲区剩余的可写空间大小 不一定能完全存储从fd读出的数据

    // 第一块缓冲区，指向可写空间
    vec[0].iov_base = begin_() + writer_idx_;
    vec[0].iov_len  = writable;
    // 第二块缓冲区，指向栈空间
    vec[1].iov_base = extrabuf.data();
    vec[1].iov_len  = sizeof(extrabuf);

    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.
    // 这里之所以说最多128k-1字节，是因为若writable为64k-1，那么需要两个缓冲区 第一个64k-1 第二个64k 所以做多128k-1
    // 如果第一个缓冲区>=64k 那就只采用一个缓冲区 而不使用栈空间extrabuf[65536]的内容
    const auto iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const auto n      = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了
    {
        hasWritten_(n);
    }
    else // extrabuf里面也写入了n-writable长度的数据
    {
        hasWritten_(writable);
        // 对buffer_扩容 并将extrabuf存储的另一部分数据追加至buffer_
        append(extrabuf.data(), n - writable); 
    }
    return n;
}

// inputBuffer_.readFd表示将对端数据读到inputBuffer_中，移动writerIndex_指针
// outputBuffer_.writeFd标示将数据写入到outputBuffer_中，从readerIndex_开始，可以写readableBytes()个字节
auto Buffer::writeFd(int fd, int* saveErrno)
    -> ssize_t
{
    auto n = ::write(fd, getReadPos_(), getReadableBytesCount());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n > 0)
    {
        // const_cast用于移除const属性，以便调用非const成员函数
        retrieveN_(n);
    }
    return n;
}

void Buffer::makeSpace_(size_t len)
{
    /**
     * | kCheapPrepend |xxx| reader | writer |                     // xxx标示reader中已读的部分
     * | kCheapPrepend | reader ｜          len          |
     **/
    if (getWritableBytesCount() + getPrependableBytesCount() < len + kCheapPrepend) // 也就是说 len > xxx + writer的部分
    {
        buffer_.resize(writer_idx_ + len);
    }
    else // 这里说明 len <= xxx + writer 把reader搬到从xxx开始 使得xxx后面是一段连续空间
    {
        size_t readable = getReadableBytesCount(); // readable = reader的长度
        std::copy(begin_() + reader_idx_,
                  begin_() + writer_idx_, // 把这一部分数据拷贝到begin+kCheapPrepend起始处
                  begin_() + kCheapPrepend);
        reader_idx_ = kCheapPrepend;
        writer_idx_ = reader_idx_ + readable;
    }
}

void Buffer::ensureWritableBytes_(size_t len)
{
    if (getWritableBytesCount() < len)
    {
        makeSpace_(len); // 扩容
    }
    assert(getWritableBytesCount() >= len);
}