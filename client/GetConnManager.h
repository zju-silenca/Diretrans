#ifndef _GET_CONNMANAGER_H_
#define _GET_CONNMANAGER_H_

#include "utils/Header.h"
#include "netlib/UdpCom.h"
#include "netlib/TimerId.h"
#include "ConnManager.h"
#include "GetFileManager.h"

class DiretransClient;

class GetConnManager : public ConnManager
{
public:
    GetConnManager(DiretransClient* client);
    ~GetConnManager();
    void getFile(sharecode code);
    void chatMsg(Buffer& msg);
    int getFd() { return conn_.getFd(); }

private:
    typedef enum ConnState_ : uint8_t
    {
        IDLE, // 空闲
        WAIT_CODE, // 等待分享码
        WAIT_SENDER, // 等待发送方地址
        CONN_SENDER, // 连接发送方
        RECV_FILE, // 接收文件
    } ConnState;

    void getFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time);
    void holdConn(sockaddr &addr, uint8_t sign);
    void sendRetranReq();
    void sendPackAck(uint32_t nums);
    void checkPeerAlive();
    void closeAllTimer();
    void closeSelf();

    DiretransClient* client_;
    UdpCom conn_;
    ConnState state_;
    sharecode code_;
    FileData fileData_;
    struct sockaddr serveraddr_;
    struct sockaddr recveraddr_;
    std::vector<TimerId> repeattimers_;
    TimerId heartBeatTimerId_;
    std::unique_ptr<GetFileManager> getManager_;
    uint32_t recvWindow_;
    bool isWindowFull_;
    Timestamp lastServerHelloTime_;
    Timestamp lastPeerHelloTime_;
    Timestamp lastDataStreamTime_;
};

#endif