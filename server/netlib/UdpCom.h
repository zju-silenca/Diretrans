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

    //int getFd() {return sockfd_;}
    uint16_t getPort() {return port_;}
    int send(Buffer& buf, struct sockaddr& target);
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb;}

private:

    static const size_t kBufferSize = 65535;
    void handleRead();
    MessageCallback messageCallback_;
    // closeCallback is no-need becauese udp is no-connection
    int sockfd_;
    uint16_t port_;
    EventLoop* loop_;
    Buffer tmpbuf_;
    std::unique_ptr<Channel> channel_;
};

#endif