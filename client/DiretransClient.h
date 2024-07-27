#ifndef _DIRETRANS_CLIENT_H
#define _DIRETRANS_CLIENT_H

#include <memory>
#include <map>
#include "Header.h"
#include "netlib/EventLoop.h"
#include "netlib/UdpCom.h"

class DiretransClient
{
public:
    DiretransClient(){};

private:

    EventLoop loop_;
    std::unique_ptr<UdpCom> conn_; 

};

#endif