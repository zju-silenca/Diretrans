#include <random>

#include "DiretransClient.h"


static uint16_t createRandomPort()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 60000);

    uint16_t res = static_cast<uint16_t>(dis(gen));
    LOG("Bind random port:%u", res);
    return res;
}

DiretransClient::DiretransClient()
{
    createSocketPair();
    do
    {
        if (conn_.get() != nullptr)
        {
            conn_.reset();
        }
        conn_ = std::make_unique<UdpCom>(&loop_, createRandomPort());
    } while (!conn_->isBound());
    cmdchannel_ = std::make_unique<Channel>(&loop_, readsock_);
    cmdchannel_->setReadCallback(std::bind(&DiretransClient::handleCmd, this));
    cmdchannel_->enableReading();
}

int DiretransClient::writeCmdtoSock(char *msg, int lenth)
{
    if (msg == nullptr)
    {
        LOG("Nullptr");
        return -1;
    }
    int res = ::write(writesock_, msg, lenth);
    return res;
}

void DiretransClient::createSocketPair()
{
    int sockfds[2] = {0};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfds))
    {
        LOG("Create sockets error.");
        abort();
        return;
    }
    readsock_ = sockfds[0];
    writesock_ = sockfds[1];
}

void DiretransClient::handleCmd()
{
    char buf[1024] = {0};
    int res = ::read(readsock_, buf, sizeof(buf));

    LOG("Receive %d cmd: %s", res, buf);
    
}
