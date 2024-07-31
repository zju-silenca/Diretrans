#ifndef _CONNMANAGER_H_
#define _CONNMANAGER_H_

#include "utils/Header.h"
#include "netlib/UdpCom.h"
#include "netlib/TimerId.h"

class DiretransClient;

typedef enum ConnState_ : uint8_t
{
    IDLE, // 空闲
    WAIT_CODE, // 等待分享码
    WAIT_SENDER, // 等待发送方地址
    WAIT_RECVER, // 等待接收方地址
    CONN_SENDER, // 连接发送方
    CONN_RECVER, // 连接接收方
    SEND_FILE, // 发送文件
    RECV_FILE, // 接收文件
} ConnState;

class ConnManager
{
public:
    ConnManager(DiretransClient* client);
    ~ConnManager();
    void shareFile(Buffer& fileName);
    void getFile(sharecode code);
    void chatMsg(Buffer& msg);
    int getFd() { return conn_.getFd(); }

private:
    void shareFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time);
    void getFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time);
    void holdConn(sockaddr &addr, uint8_t sign);

    void closeSelf();

    DiretransClient* client_;
    UdpCom conn_;
    ConnState state_;
    sharecode code_;
    struct sockaddr serveraddr_;
    struct sockaddr recveraddr_;
    std::vector<TimerId> repeattimers_;
};

#endif