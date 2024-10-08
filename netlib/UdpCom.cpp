#include "UdpCom.h"
#include "Log.h"
#include "Channel.h"
#include "Timestamp.h"
#include "EventLoop.h"

#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

static int createSocket()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        LOG("Create socket error");
        abort();
    }

    /* 设置非阻塞模式
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        LOG("fcntl F_GETFL failed");
        close(sockfd);
        abort();
    }

    flags = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (flags < 0) {
        LOG("fcntl F_SETFL failed");
        close(sockfd);
        abort();
    }*/

    return sockfd;
}

UdpCom::UdpCom(EventLoop* loop, uint16_t port)
    : bound_(false),
     port_(port),
     loop_(loop),
     sockfd_(createSocket()),
     channel_(new Channel(loop_, sockfd_))
{
    bindPort(port);
    tmpbuf_.ensureWritableBytes(kBufferSize);
    channel_->setReadCallback(std::bind(&UdpCom::handleRead, this));
    channel_->enableReading();
}

UdpCom::~UdpCom()
{
    channel_->disableAll();
    channel_->remove();
    if (sockfd_ > 0)
    {
        close(sockfd_);
    }
}

void UdpCom::bindPort(uint16_t port)
{
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (0 > bind(sockfd_, (const sockaddr *)&server_addr, sizeof(server_addr)))
    {
        LOG("Bind socket port %u error", port);
        bound_ = false;
    }
    else
    {
        bound_ = true;
    }
    LOG("Bind socket port %u success.", port);
}

void UdpCom::send(const Buffer &buf, const sockaddr &target)
{
    assert(bound_ == true);
    //loop_->runInThreadPool(std::bind(&UdpCom::sendInThreadPool, this, buf, target));
    // 使用线程池会降低吞吐量
    sendInThreadPool(buf, target);
}

void UdpCom::handleRead()
{
    struct sockaddr sourceAddr = {0};
    socklen_t len = sizeof(struct sockaddr);
    Timestamp timestamp(Timestamp::now());

    assert(tmpbuf_.writableBytes() >= kBufferSize);
    ssize_t n = ::recvfrom(sockfd_, tmpbuf_.beginWrite(), kBufferSize, MSG_DONTWAIT, &sourceAddr, &len);

    if (n >= static_cast<ssize_t>(kBufferSize))
    {
        LOG("recvfrom %ld bytes, may out of buffer.", n);
    }
    else
    {
        //LOG("recvfrom %ld bytes.", n);
    }
    tmpbuf_.hasWritten(n);
    if (messageCallback_)
    {
        messageCallback_(this, tmpbuf_, sourceAddr, timestamp);
    }
    tmpbuf_.retrieveAll();
}

void UdpCom::sendInThreadPool(const Buffer &buf, const sockaddr &target)
{
    assert(bound_ == true);
    ssize_t n = ::sendto(sockfd_, buf.peek(), buf.readableBytes(), 0, &target, sizeof(target));
    if (n == -1)
    {
        LOG("sendto return -1, error no: %d", errno);
    }
    if (n != static_cast<ssize_t>(buf.readableBytes()))
    {
        LOG("Sendto %lu bytes but buf have %lu bytes", n, buf.readableBytes());
    }
    //LOG("Sendto %lu bytes", n);
    return;
}
