#ifndef _DIRETRANS_SERVER_H_
#define _DIRETRANS_SERVER_H_

#include <memory>
#include <map>
#include "utils/Header.h"
#include "netlib/EventLoop.h"
#include "netlib/UdpCom.h"

class Buffer;

class RandomCode
{
public:
    RandomCode();
    uint32_t getCode();

private:
    static const int kCodeNums = 1 * 100 * 1000;

    void genCodes();
    std::vector<uint32_t> codes_;
    int startIndex_;
    int endIndex_;
};

class DiretransServer
{
public:
    DiretransServer(int port);
    void start();

private:
    static const int kMaxSharedPerIp = 10;
    uint32_t getSockaddrIp(const sockaddr& addr);
    void handleMessage(UdpCom* , Buffer&, sockaddr&, Timestamp&);
    void handleHello();
    void handleShareFile(const Buffer& req, const sockaddr& addr);
    void handleGetFile(const Buffer& req, const sockaddr& recvAddr);
    EventLoop loop_;
    RandomCode shareCodes_;
    std::unique_ptr<UdpCom> conn_;
    // 记录分享码对应的文件信息
    std::map<sharecode, FileDataAddr> fileDatas_;
    // 记录某个ip分享的文件数量
    std::map<uint32_t, int> shareFileNum_;
};

#endif