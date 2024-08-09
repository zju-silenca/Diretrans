#ifndef _SEND_CONNMANAGER_H_
#define _SEND_CONNMANAGER_H_

#include "utils/Header.h"
#include "netlib/UdpCom.h"
#include "netlib/TimerId.h"
#include "ConnManager.h"
#include "SendFileManager.h"

class DiretransClient;


class SendConnManager : public ConnManager
{
public:
    SendConnManager(DiretransClient* client);
    ~SendConnManager();
    void shareFile(Buffer& fileName);
    void chatMsg(Buffer& msg);
    int getFd() { return conn_.getFd(); }

private:
    typedef enum ConnState_ : uint8_t
    {
        IDLE, // 空闲
        WAIT_CODE, // 等待分享码
        WAIT_RECVER, // 等待接收方地址
        CONN_RECVER, // 连接接收方
        SEND_FILE, // 发送文件
    } ConnState;

    void shareFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time);
    void holdConn(sockaddr &addr, uint8_t sign);
    void sendFileStream();
    void sendLossPieces(std::vector<uint32_t> pieces);
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
    std::unique_ptr<SendFileManager> sendManager_;
    uint32_t sendWindow_;
    bool isWindowFull_;
    Timestamp lastServerHelloTime_;
    Timestamp lastPeerHelloTime_;
};

#endif