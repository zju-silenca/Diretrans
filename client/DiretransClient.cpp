#include <random>
#include <iostream>
#include <vector>
#include <string>
#include <string.h>
#include <arpa/inet.h>

#include "DiretransClient.h"

using namespace std::placeholders;

DiretransClient::DiretransClient()
{
    struct sockaddr_in* addr_in = (struct sockaddr_in*)&serveraddr_;
    createSocketPair();
    cmdchannel_ = std::make_unique<Channel>(&loop_, readsock_);
    cmdchannel_->setReadCallback(std::bind(&DiretransClient::handleCmd, this));
    cmdchannel_->enableReading();

    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(0x4E13);
    addr_in->sin_addr.s_addr = htonl(0x23EC821Cu);
}

int DiretransClient::writeCmdtoSock(const char *msg, int lenth)
{
    if (msg == nullptr)
    {
        LOG("Nullptr");
        return -1;
    }
    int res = ::write(writesock_, msg, lenth);
    return res;
}

void DiretransClient::movePendConn(int fd, sharecode code)
{
    auto conn = pendingconns_.find(fd);
    if (conn == pendingconns_.end())
    {
        LOG("No such conn. fd: %d", fd);
        return;
    }

    assert(sendconns_.find(code) == sendconns_.end());

    sendconns_[code] = std::move(conn->second);
    pendingconns_.erase(conn);
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
    std::vector<std::string> cmds;
    std::string temp;

    int res = ::read(readsock_, buf, sizeof(buf));
    LOG("Receive %d cmd: %s", res, buf);

    std::istringstream iss(buf);
    while (iss >> temp)
    {
        cmds.push_back(temp);
    }

    if (cmds.size() <= 1)
    {
        LOG("Empty cmd or arg less.");
        return;
    }

    if ("SHARE" == cmds[0])
    {
        Buffer fileName;
        fileName.append(cmds[1]);
        auto conn = std::make_unique<ConnManager>(this);
        int fd = conn->getFd();
        pendingconns_[fd] = std::move(conn);
        pendingconns_[fd]->shareFile(fileName);
    }else if ("GET" == cmds[0])
    {

    }else if ("CHAT" == cmds[0])
    {

    } else
    {
        LOG("Error cmd.");
        return;
    }

}