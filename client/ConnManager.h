#ifndef _CONNMANAGER_H_
#define _CONNMANAGER_H_

#include "utils/Header.h"
#include "netlib/UdpCom.h"

class DiretransClient;

typedef enum ConnState_ : uint8_t
{
    IDLE, // 空闲
    WAIT_CODE, // 等待分享码
    WAIT_PEER, // 等待对端地址
    WAIT_CONN, // 等待连接
    CONN_PEER, // 连接对端
    HOLD_CONN, // 心跳保持连接
    SEND_FILE, // 发送文件
    RECV_FILE, // 接收文件
} ConnState;

class ConnManager
{
public:
    ConnManager(DiretransClient* client);
    ~ConnManager() = default;
    void shareFile(Buffer& fileName);
    void getFile(sharecode code);
    int getFd() { return conn_.getFd(); }

private:
    void shareFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time);

    DiretransClient* client_;
    UdpCom conn_;
    ConnState state_;
    sharecode code_;
    struct sockaddr serveraddr_;
};

#endif