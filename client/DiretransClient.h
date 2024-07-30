#ifndef _DIRETRANS_CLIENT_H
#define _DIRETRANS_CLIENT_H

#include <memory>
#include <map>
#include <set>
#include "utils/Header.h"
#include "netlib/EventLoop.h"
#include "netlib/Channel.h"
#include "netlib/UdpCom.h"
#include "ConnManager.h"

class DiretransClient
{
public:
    DiretransClient();
    int writeCmdtoSock(const char* msg, int lenth);
    void movePendConn(int fd, sharecode code);
    void closeConn(int fd);
    void start() { loop_.loop(); }
    EventLoop* getLoop() { return &loop_; }
    struct sockaddr getServerAddr() const { return serveraddr_; }

private:
    void createSocketPair();
    void handleCmd();
    EventLoop loop_;

    int readsock_;
    int writesock_;
    std::unique_ptr<Channel> cmdchannel_;
    // 等待服务器返回分享码的连接 key为fd
    std::map<int, std::unique_ptr<ConnManager>> pendingconns_;
    // 发送文件的连接
    std::map<sharecode, std::unique_ptr<ConnManager>> sendconns_;
    // 获取文件的连接
    std::map<sharecode, std::unique_ptr<ConnManager>> getconns_;
    struct sockaddr serveraddr_;
};

#endif