#ifndef _UDP_H_
#define _UDP_H_

#include "Callback.h"
#include "Buffer.h"

#include <unistd.h>
#include <sys/socket.h>
#include <memory>

class Channel;
class EventLoop;
class Timestamp;

class UdpCom
{
public:
    typedef std::function<void()> EventCallback;
    typedef std::function<void(UdpCom* , Buffer&, struct sockaddr&, Timestamp&)> MessageCallback;
    UdpCom(EventLoop* loop, uint16_t port);
    UdpCom(const UdpCom&) = delete;
    ~UdpCom();

    int getFd() {return sockfd_;}
    bool isBound() { return bound_; }
    void bindPort(uint16_t port);
    uint16_t getPort() {return port_;}
    void send(const Buffer& buf, const struct sockaddr& target);
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb;}
    EventLoop* getLoop() { return loop_; }

private:

    static const size_t kBufferSize = 65535;
    void handleRead();
    void sendInThreadPool(const Buffer& buf, const struct sockaddr& target);
    MessageCallback messageCallback_;
    // closeCallback is no-need becauese udp is no-connection
    bool bound_;
    uint16_t port_;
    EventLoop* loop_;
    int sockfd_;
    std::unique_ptr<Channel> channel_;
    Buffer tmpbuf_;
};

#endif