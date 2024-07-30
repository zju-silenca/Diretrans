#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <algorithm>
#include <vector>
#include <string>

#include <assert.h>

class Buffer
{
public:
    static const size_t kPrependSize = 8;
    static const size_t kInitSize = 1024;

    Buffer()
    : buffer_(kPrependSize + kInitSize, 0),
    readIndex_(kPrependSize),
    writeIndex_(kPrependSize)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == kInitSize);
        assert(prependableBytes() == kPrependSize);
    }

    void swap(Buffer& buf)
    {
        buffer_.swap(buf.buffer_);
        std::swap(readIndex_, buf.readIndex_);
        std::swap(writeIndex_, buf.writeIndex_);
    }

    size_t readableBytes() const { return writeIndex_ - readIndex_; }
    size_t writableBytes() const { return buffer_.size() - writeIndex_; }
    size_t prependableBytes() const { return readIndex_; }

    const char* peek() const { return begin() + readIndex_; }
    const char* beginWrite() const { return begin() + writeIndex_; }
    char* beginWrite() { return begin() + writeIndex_; }
    void hasWritten(size_t len) { writeIndex_ += len; }

    void retrieve(size_t len)
    {
        assert(len <= readableBytes());
        readIndex_ += len;
    }

    void retrieveUntil(const char* end)
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    void retrieveAll()
    {
        readIndex_ = kPrependSize;
        writeIndex_ = kPrependSize;
    }

    std::string retrieveAsString()
    {
        std::string str(peek(), readableBytes());
        retrieveAll();
        return str;
    }

    void append(std::string& str)
    {
        append(str.data(), str.size());
    }

    void append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        hasWritten(len);
    }

    void prepend(const void* data, size_t len)
    {
        assert(len <= prependableBytes());
        readIndex_ -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d+len, begin() + readIndex_);
    }

    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            expendSpace(len);
        }
        assert(writableBytes() >= len);
    }

private:
    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }

    void expendSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kPrependSize)
        {
            buffer_.resize(writeIndex_ + len);
        }
        else
        {
            assert(kPrependSize < readIndex_);
            size_t readable = readableBytes();
            std::copy(begin()+readIndex_, begin()+writeIndex_, begin()+kPrependSize);
            readIndex_ = kPrependSize;
            writeIndex_ = readIndex_ + readable;
            assert(readable == readableBytes());
        }
    }

    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;
};

#endif