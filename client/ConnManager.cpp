#include "DiretransClient.h"
#include "ConnManager.h"

#include <random>
#include <string.h>
#include <arpa/inet.h>

using namespace std::placeholders;

static uint16_t getRandomPort()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 60000);

    uint16_t res = static_cast<uint16_t>(dis(gen));
    LOG("Bind random port:%u", res);
    return res;
}

ConnManager::ConnManager(DiretransClient *client)
    : client_(client),
    conn_(client_->getLoop(), getRandomPort()),
    state_(IDLE),
    code_(0),
    serveraddr_(client_->getServerAddr())
{
    while (!conn_.isBound())
    {
        conn_.bindPort(getRandomPort());
    }
}

void ConnManager::shareFile(Buffer &fileName)
{
    Buffer buf;
    Header header;
    FileData fileData;

    assert(state_ == IDLE);
    assert(conn_.isBound());

    memcpy(fileData.fileName, fileName.peek(), fileName.readableBytes());
    header.sign = kServerSign;
    header.type = SHARE_FILE;
    header.dataLenth = sizeof(FileData);

    buf.append((char *)&header, sizeof(Header));
    buf.append((char *)&fileData, sizeof(Header));

    conn_.setMessageCallback(std::bind(
        &ConnManager::shareFileMsgHandle,
        this, _1, _2, _3, _4));
    conn_.send(buf, serveraddr_);
    state_ = WAIT_CODE;
}

void ConnManager::getFile(sharecode code)
{
}

void ConnManager::shareFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time)
{
    Header header;
    Buffer sendMsg;
    ShareCode code;
    uint8_t sign = 0;
    if (conn == nullptr)
    {
        LOG("nullptr");
    }

    char ip_str[INET_ADDRSTRLEN];
    const struct sockaddr_in *sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, &sa_in->sin_addr.s_addr, ip_str, INET_ADDRSTRLEN);
    LOG("New message From %s:%u", ip_str, ::ntohs(sa_in->sin_port));

    if (buf.readableBytes() < sizeof(Header))
    {
        LOG("Receive %lu bytes, lower than Header.", buf.readableBytes());
        return;
    }

    sign = (uint8_t)(*buf.peek());
    if (sign != kServerSign && sign != kClientSign)
    {
        LOG("Header sign is %#x.", sign);
        return;
    }

    memcpy(&header, buf.peek(), sizeof(Header));
    switch (header.type)
    {
    case HELLO:
        header.sign = sign;
        header.type = HELLO_REPLY;
        header.dataLenth = 0;
        sendMsg.append((char *)&header, sizeof(header));
        conn_.send(sendMsg, addr);
        break;
    case SHARE_CODE:
        if (state_ != WAIT_CODE)
        {
            LOG("State %u error", state_);
            break;
        }
        memcpy(&code, buf.peek()+sizeof(Header), sizeof(ShareCode));
        code_ = code.code;
        client_->movePendConn(conn_.getFd(), code_);
        state_ = WAIT_CONN;
        break;

    default:
        break;
    }
}
