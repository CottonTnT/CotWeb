#pragma once
#include "net/Endian.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

// 网络库底层的缓冲区类型定义

// muduo::net::Buffer 的核心目标是提供一个可动态增长的缓冲区，用于暂存网络套接字读写的数据。其设计深受 Netty ChannelBuffer 的启发，采用了经典的三段式内存布局：

// +-------------------+------------------+------------------+
// | prependable bytes | readable bytes | writable bytes |
// | | (CONTENT) | |
// +-------------------+------------------+------------------+
// | | | |
// 0 <= readerIndex <= writerIndex <= size

// prependable bytes (可预置空间): 位于缓冲区的最前端。它的一个妙用是在已有数据前方便地添加协议头（如消息长度），而无需移动现有数据。muduo 默认预留了 kCheapPrepend = 8 字节。
// readable bytes (可读数据区): 存储了从网络接收到或准备发送的实际有效数据，从 readerIndex_ 开始，到 writerIndex_ 结束。
// writable bytes (可写空间): 位于可读数据之后，用于追加新的数据，从 writerIndex_ 开始，到缓冲区末尾结束。

class Buffer {
public:
    inline static const size_t kCheapPrepend = 8;
    inline static const size_t kInitialSize  = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , reader_idx_(kCheapPrepend)
        , writer_idx_(kCheapPrepend)
    {
        assert(getReadableBytesCount() == 0);
        assert(getWritableBytesCount() == initialSize);
        assert(getPrependableBytesCount() == kCheapPrepend);
    }

    Buffer(const Buffer&) = default;
    auto operator=(const Buffer&)
        -> Buffer& = default;

    Buffer(Buffer&&) noexcept;
    auto operator=(Buffer&&) noexcept
        -> Buffer&;
    ~Buffer() = default;

    [[nodiscard]] auto getReadableBytesCount() const
        -> size_t; 
    [[nodiscard]] auto getWritableBytesCount() const
        -> size_t; 
    [[nodiscard]] auto getPrependableBytesCount() const
        -> size_t ;

    // 返回缓冲区中可读数据的起始地址

    static std::string c_CRLF;

    [[nodiscard]] auto getReadableSpan() const
        -> std::span<char>
    {
        return std::span<char> {(char*)getReadPos_(), getReadableBytesCount()};
    }

    [[nodiscard]] auto getReadableSV() const
        -> std::string_view
    {
        return std::string_view{(char*)getReadPos_(), getReadableBytesCount()};
    }

    [[nodiscard]] auto findCrLf() const
        -> const unsigned char*;

    auto findCrLf(const unsigned char* stPos) const
        -> const unsigned char*;

    [[nodiscard]] auto findEol() const
        -> const char*;

    auto findEol(const unsigned char* stPos) const
        -> const char*;

    // 把onMessage函数上报的Buffer数据 转成string类型的数据返回
    auto readAllAsString()
        -> std::string { return readNAsString(getReadableBytesCount()); }

    auto readAllAndDiscard()
        -> void { retrieveAll_(); }

    auto readNAsString(size_t len)
        -> std::string
    {
        // auto readableSpan = 
        auto result = std::string(reinterpret_cast<char*>(getReadPos_()), len);
        retrieveN_(len); // 上面一句把缓冲区中可读的数据已经读取出来 这里肯定要对缓冲区进行复位操作
        return result;
    }

    // buffer_.size - writerIndex_
    void append(std::string_view str)
    {
        append(str.data(), str.size());
    }

    void append(std::span<unsigned char> charSpan)
    {

        ensureWritableBytes_(charSpan.size());
        std::copy(charSpan.begin(), charSpan.end(), getWritePos_());
        hasWritten_(charSpan.size());
    }

    void append(std::span<char> charSpan)
    {

        ensureWritableBytes_(charSpan.size());
        std::copy(charSpan.begin(), charSpan.end(), getWritePos_());
        hasWritten_(charSpan.size());
    }

    // 把[data, data+len]内存上的数据添加到writable缓冲区当中
    void append(const unsigned char* data, size_t len)
    {
        ensureWritableBytes_(len);
        std::copy(data, data + len, getWritePos_());
        hasWritten_(len);
    }
    
    void append(const void* /*restrict*/ data, size_t len)
    {
        append(static_cast<const unsigned char*>(data), len);
    }


    ///
    /// Append a integer using network endian
    ///
    template <typename Integer>
        requires std::is_integral_v<Integer>
    void appendInteger(Integer x)
    {
        if constexpr (sizeof(Integer) == 8)
        {
            auto be64 = Sock::HostToNetwork64(static_cast<uint64_t>(x));
            append(&be64, sizeof be64);
        }
        else if constexpr (sizeof(Integer) == 4)
        {
            auto be32 = Sock::hostToNetwork32(static_cast<uint32_t>(x));
            append(&be32, sizeof be32);
        }
        else if constexpr (sizeof(Integer) == 2)
        {
            auto be16 = Sock::HostToNetwork16(static_cast<uint16_t>(x));
            append(&be16, sizeof be16);
        }
        else if constexpr (sizeof(Integer) == 1)
        {
            append(&x, sizeof x);
        }
    }

    ///
    /// Read a integer from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int32_t)
    template <typename Integer>
        requires std::is_integral_v<Integer>
    auto readInteger()
        -> Integer
    {
        auto result = peekInteger<Integer>();
        retrieveN_(sizeof(Integer));
        return result;
    }

    ///
    /// Peek a integer from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(integer)

    template <typename Integer>
        requires std::is_integral_v<Integer>
    [[nodiscard]] auto peekInteger() const
        -> Integer
    {
        assert(getReadableBytesCount() >= sizeof(Integer));
        auto rst = Integer{};
        ::memcpy(&rst, getReadPos_(), sizeof rst);
        if constexpr (sizeof(Integer) == 8)
        {
            return Sock::NetworkToHost64(rst);
        }
        else if constexpr (sizeof(Integer) == 4)
        {
            return Sock::NetworkToHost32(rst);
        }
        else if constexpr (sizeof(Integer) == 2)
        {
            return Sock::networkToHost16(rst);
        }
        else if constexpr (sizeof(Integer) == 1)
        {
            return rst;
        }
    }

    ///
    /// Prepend int64_t using network endian
    ///
    void PrependInt64(int64_t x)
    {
        int64_t be64 = Sock::HostToNetwork64(x);
        Prepend(&be64, sizeof be64);
    }

    ///
    /// Prepend int32_t using network endian
    ///
    void PrependInt32(int32_t x)
    {
        int32_t be32 = Sock::hostToNetwork32(x);
        Prepend(&be32, sizeof be32);
    }

    void PrependInt16(int16_t x)
    {
        int16_t be16 = Sock::HostToNetwork16(x);
        Prepend(&be16, sizeof be16);
    }

    void PrependInt8(int8_t x)
    {
        Prepend(&x, sizeof x);
    }
    void Prepend(const void* /*restrict*/ data, size_t len)
    {
        assert(len <= getPrependableBytesCount());
        reader_idx_ -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d + len, begin_() + reader_idx_);
    }

    [[nodiscard]] auto toReadableSpan()
        -> std::span<unsigned char>;
    [[nodiscard]] auto toReadableSV()
        -> std::string_view;
    void swap(Buffer& rhs);

    void Shrink(size_t reserve)
    {
        // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
        auto other = Buffer{};
        other.ensureWritableBytes_(getReadableBytesCount() + reserve);
        other.append(toReadableSpan());
        swap(other);
    }

    [[nodiscard]] auto InternalCapacity() const
        -> size_t
    {
        return buffer_.capacity();
    }

    // 从fd上读取数据
    auto readFd(int fd, int* saveErrno)
        -> ssize_t;
    // 通过fd发送数据
    auto writeFd(int fd, int* saveErrno)
        -> ssize_t;

private:
    // vector底层数组首元素的地址 也就是数组的起始地址

    [[nodiscard]] auto begin_()
        -> unsigned char*
    {
        return buffer_.data();
    }
    [[nodiscard]] auto begin_() const
        -> const unsigned char*
    {
        return buffer_.data();
    }

    [[nodiscard]] auto getReadPos_() const
        -> const unsigned char*; 

    [[nodiscard]] auto getReadPos_() 
        -> unsigned char*; 

    auto getWritePos_()
        ->  unsigned char*;
    [[nodiscard]] auto getWritePos_() const
        -> const unsigned char*; 

    void makeSpace_(size_t len);


    void ensureWritableBytes_(size_t len);

    /**
     * @brief 读取buffer后，调整buffer的读写指针
     */
    void retrieveN_(size_t len)
    {
        if (len < getReadableBytesCount())
        {
            reader_idx_ += len; // 说明应用只读取了可读缓冲区数据的一部分，就是len长度 还剩下readerIndex+=len到writerIndex_的数据未读
        }
        else // len >= readableBytes()
        {
            retrieveAll_();
        }
    }
    void retrieveAll_()
    {
        reader_idx_ = kCheapPrepend;
        writer_idx_ = kCheapPrepend;
    }
    void retrieveUntil_(const unsigned char* end)
    {
        assert(getReadPos_() <= end);
        assert(end <= getWritePos_());
        retrieveN_(end - getReadPos_());
    }

    void hasWritten_(size_t len)
    {
        assert(len <= getWritableBytesCount());
        writer_idx_ += len;
    }

    void unWrite_(size_t len)
    {
        assert(len <= getReadableBytesCount());
        writer_idx_ -= len;
    }

    std::vector<unsigned char> buffer_;
    size_t reader_idx_;
    size_t writer_idx_;
};

inline void swap(Buffer& lhs, Buffer& rhs)
{
    lhs.swap(rhs);
}