#ifndef _DIRETRANS_CLIENT_H
#define _DIRETRANS_CLIENT_H

#include <memory>
#include <map>
#include "utils/Header.h"
#include "netlib/EventLoop.h"
#include "netlib/Channel.h"
#include "netlib/UdpCom.h"

class DiretransClient
{
public:
    DiretransClient();
    int writeCmdtoSock(char* msg, int lenth);
    void start() { loop_.loop(); }

private:
    void createSocketPair();
    void handleCmd();
    EventLoop loop_;
    std::unique_ptr<UdpCom> conn_;
    int readsock_;
    int writesock_;
    std::unique_ptr<Channel> cmdchannel_;
};

#endif