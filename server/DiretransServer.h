#ifndef _DIRETRANS_SERVER_H_
#define _DIRETRANS_SERVER_H_

#include <memory>
#include <map>
#include <unordered_map>
#include <string.h>
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
    static const int kTimeoutSec = 5 * 60;
    uint32_t getSockaddrIp(const sockaddr& addr);
    void handleMessage(UdpCom* , Buffer&, sockaddr&, Timestamp&);
    void handleHello(const sockaddr& addr);
    void handleShareFile(const Buffer& req, const sockaddr& addr);
    void handleGetFile(const Buffer& req, const sockaddr& recvAddr);
    void removeTimeoutConn();
    EventLoop loop_;
    RandomCode shareCodes_;
    std::unique_ptr<UdpCom> conn_;
    // 记录分享码对应的文件信息
    std::map<sharecode, FileDataAddr> fileDatas_;
    // 记录每个连接分配的分享码
    std::map<sockaddr, sharecode> codesMap_;
    // 记录每个连接上次活跃时间
    std::map<sockaddr, Timestamp> activeTimes_;
    // 记录某个ip分享的文件数量
    std::map<uint32_t, int> shareFileNum_;
};

inline bool operator==(sockaddr l, sockaddr r)
{
    return (0 == memcmp(&l, &r, sizeof(sockaddr)));
}

inline bool operator<(sockaddr l, sockaddr r)
{
    if (l.sa_family < r.sa_family)
    {
        return true;
    }else if (l.sa_family > r.sa_family)
    {
        return false;
    }

    for (size_t i = 0; i < 14; ++i)
    {
        if (l.sa_data[i] < r.sa_data[i])
        {
            return true;
        } else if (l.sa_data[i] > r.sa_data[i])
        {
            return false;
        }
    }
    return false;
}

#endif