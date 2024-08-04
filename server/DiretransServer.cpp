#include <algorithm>
#include <numeric>
#include <random>
#include <functional>
#include <string.h>
#include <arpa/inet.h>
#include "DiretransServer.h"


using namespace std::placeholders;

RandomCode::RandomCode()
{
    genCodes();
}

uint32_t RandomCode::getCode()
{
    if (endIndex_ <= startIndex_)
    {
        genCodes();
    }
    uint32_t res = codes_[startIndex_];
    ++startIndex_;
    return res;
}

void RandomCode::genCodes()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    codes_.resize(kCodeNums);
    std::iota(codes_.begin(), codes_.end(), kCodeNums);
    std::shuffle(codes_.begin(), codes_.end(), gen);
    startIndex_ = 0;
    endIndex_ = codes_.size() - 1;
}

DiretransServer::DiretransServer(int port)
    : conn_(new UdpCom(&loop_, port))
{
    conn_->setMessageCallback(std::bind(&DiretransServer::handleMessage, this, _1, _2, _3, _4));
}

void DiretransServer::start()
{
    removeUnliveConnTimer_ = loop_.runEvery(kTimeoutSec, std::bind(&DiretransServer::removeTimeoutConn, this));
    loop_.loop();
}

uint32_t DiretransServer::getSockaddrIp(const sockaddr &addr)
{
    struct sockaddr_in *sa_in = (struct sockaddr_in *)&addr;
    uint32_t ip = sa_in->sin_addr.s_addr;
    return ip;
}

void DiretransServer::handleMessage(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time)
{
    Header header;
    Buffer sendMsg;
    if (conn == nullptr)
    {
        LOG("nullptr");
    }
    assert(conn == conn_.get());

    char ip_str[INET_ADDRSTRLEN];
    const struct sockaddr_in *sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, &sa_in->sin_addr.s_addr, ip_str, INET_ADDRSTRLEN);

    LOG("New message From %s:%u", ip_str, ::ntohs(sa_in->sin_port));

    if (buf.readableBytes() < sizeof(Header))
    {
        LOG("Receive %lu bytes, lower than Header.", buf.readableBytes());
        return;
    }
    if ((uint8_t)(*buf.peek()) != kServerSign)
    {
        LOG("Header sign is %u != %u.", (uint8_t)(*buf.peek()), kServerSign);
        return;
    }

    activeTimes_[addr] = time;

    memcpy(&header, buf.peek(), sizeof(Header));
    switch (header.type)
    {
    case HELLO:
        handleHello(addr);
        break;
    case SHARE_FILE:
        handleShareFile(buf, addr);
        break;
    case GET_FILE:
        handleGetFile(buf, addr);
        break;
    case SHARE_CODE:
    case FILE_DATA:
    default:
        LOG("Error type: %u", header.type);
        break;
    }
}

void DiretransServer::handleHello(const sockaddr& addr)
{
    Buffer buf;
    Header header;
    header.sign = kServerSign;
    header.type = HELLO_REPLY;
    header.dataLenth = 0;
    buf.ensureWritableBytes(sizeof(header));
    memcpy(buf.beginWrite(), &header, sizeof(header));
    buf.hasWritten(sizeof(header));
    conn_->send(buf, addr);
}

void DiretransServer::handleShareFile(const Buffer& req, const sockaddr& addr)
{
    Buffer send;
    Header header = {0};
    FileDataAddr fileDataAddr = {0};
    ShareCode shareCode = {0};
    assert(req.readableBytes() > sizeof(Header));
    memcpy(&header, req.peek(), sizeof(header));
    assert(header.type == SHARE_FILE);
    if (header.dataLenth < sizeof(FileData))
    {
        LOG("dataLenth:%u error.", header.dataLenth);
        return;
    }
    memcpy(&fileDataAddr.fileData, req.peek()+sizeof(Header), sizeof(FileData));
    if (strlen(fileDataAddr.fileData.fileName) == 0 ||
        fileDataAddr.fileData.fileBytes == 0)
    {
        header.sign = kServerSign;
        header.type = ERROR_FORMAT;
        header.dataLenth = 0;
        send.append((char *)&header, sizeof(Header));
        conn_->send(send, addr);
        return;
    }

    uint32_t ip = getSockaddrIp(addr);
    if (shareFileNum_.find(ip) != shareFileNum_.end())
    {
        if (shareFileNum_[ip] >= kMaxSharedPerIp ||
            codesMap_.find(addr) != codesMap_.end())
        {
            char ip_str[INET_ADDRSTRLEN];
            struct in_addr in_ad;
            in_ad.s_addr = htonl(ip);
            inet_ntop(AF_INET, &in_ad.s_addr, ip_str, INET_ADDRSTRLEN);
            LOG("IP:%s has shared %d files, refused.", ip_str, shareFileNum_[ip]);
            header.sign = kServerSign;
            header.type = ERROR_LIMIT;
            header.dataLenth = 0;
            send.append((char *)&header, sizeof(Header));
            conn_->send(send, addr);
            return;
        }
        else
        {
            ++shareFileNum_[ip];
        }
    }
    else
    {
        shareFileNum_[ip] = 1;
    }

    do
    {
        shareCode.code = shareCodes_.getCode();
    } while (fileDatas_.find(shareCode.code) != fileDatas_.end());

    char ip_str[INET_ADDRSTRLEN];
    const struct sockaddr_in *sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, &sa_in->sin_addr.s_addr, ip_str, INET_ADDRSTRLEN);

    LOG("New share code:%u From %s:%u", shareCode.code, ip_str, ::ntohs(sa_in->sin_port));
    assert(codesMap_.find(addr) == codesMap_.end());
    codesMap_[addr] = shareCode.code;
    fileDataAddr.addr = addr;
    fileDatas_[shareCode.code] = fileDataAddr;
    header.sign = kServerSign;
    header.type = SHARE_CODE;
    header.dataLenth = sizeof(ShareCode);
    send.append((char *)&header, sizeof(Header));
    send.append((char *)&shareCode, sizeof(ShareCode));
    conn_->send(send, addr);
    return;
}

void DiretransServer::handleGetFile(const Buffer& req, const sockaddr& recvAddr)
{
    Buffer toRecv;
    Buffer toSend;
    Header header = {0};
    FileDataAddr fileDataAddr = {0};
    ShareCode shareCode = {0};
    PeerAddr peerAddr = {0};

    assert(req.readableBytes() > sizeof(Header));
    memcpy(&header, req.peek(), sizeof(header));
    assert(header.type == GET_FILE);
    if (header.dataLenth < sizeof(ShareCode))
    {
        LOG("dataLenth:%u error.", header.dataLenth);
        return;
    }
    memcpy(&shareCode, req.peek()+sizeof(Header), sizeof(ShareCode));

    if (fileDatas_.find(shareCode.code) == fileDatas_.end())
    {
        LOG("Code:%u not found.", shareCode.code);
        header.sign = kServerSign;
        header.type = ERROR_404;
        header.dataLenth = 0;
        toRecv.append((char *)&header, sizeof(Header));
        conn_->send(toRecv, recvAddr);
        return;
    }
    else
    {
        fileDataAddr = fileDatas_[shareCode.code];
    }

    header.sign = kServerSign;
    header.type = FILE_DATA;
    header.dataLenth = sizeof(FileDataAddr);
    toRecv.append((char *)&header, sizeof(Header));
    toRecv.append((char *)&fileDataAddr, sizeof(FileDataAddr));
    conn_->send(toRecv, recvAddr);

    header.sign = kServerSign;
    header.type = FILE_REQ;
    header.dataLenth = sizeof(PeerAddr);
    peerAddr.addr = recvAddr;
    toSend.append((char *)&header, sizeof(Header));
    toSend.append((char *)&peerAddr, sizeof(PeerAddr));
    conn_->send(toSend, fileDataAddr.addr);

    return;
}

void DiretransServer::removeTimeoutConn()
{
    Timestamp nowTime(Timestamp::now());
    int nums = 0;
    if (activeTimes_.size() == 0)
    {
        return;
    }

    for (auto it = activeTimes_.begin(); it != activeTimes_.end(); )
    {
        if (nowTime.getMicroSeconds() > it->second.getMicroSeconds() + kTimeoutSec * Timestamp::kMicroSecondsPerSecond)
        {
            if (codesMap_.find(it->first) != codesMap_.end())
            {
                fileDatas_.erase(codesMap_[it->first]);
                codesMap_.erase(it->first);
                --shareFileNum_[getSockaddrIp(it->first)];
                ++nums;
            }
            it = activeTimes_.erase(it);
        }else
        {
            ++it;
        }
    }
    LOG("Remove %d timeout sharers.", nums);
}
