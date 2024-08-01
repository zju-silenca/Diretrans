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

ConnManager::~ConnManager()
{
    for (auto timerId = repeattimers_.begin();
        timerId != repeattimers_.end(); ++timerId)
    {
        timerId->cancel();
    }
}

void ConnManager::shareFile(Buffer &fileName)
{
    Buffer buf;
    Header header = {0};
    FileData fileData = {0};

    assert(state_ == IDLE);
    assert(conn_.isBound());

    strncpy(fileData.fileName, fileName.peek(), fileName.readableBytes());
    fileData.fileBytes = 1000;
    fileData.fileCRC32 = 0xAABBCCDDu;
    header.sign = kServerSign;
    header.type = SHARE_FILE;
    header.dataLenth = sizeof(FileData);

    buf.append((char *)&header, sizeof(Header));
    buf.append((char *)&fileData, sizeof(FileData));

    conn_.setMessageCallback(std::bind(
        &ConnManager::shareFileMsgHandle,
        this, _1, _2, _3, _4));
    conn_.send(buf, serveraddr_);
    state_ = WAIT_CODE;
}

void ConnManager::getFile(sharecode code)
{
    Buffer buf;
    Header header = {0};
    ShareCode shareCode = {0};

    assert(state_ == IDLE);
    assert(conn_.isBound());

    code_ = code;
    shareCode.code = code;
    header.sign = kServerSign;
    header.type = GET_FILE;
    header.dataLenth = sizeof(ShareCode);

    buf.append((char *)&header, sizeof(Header));
    buf.append((char *)&shareCode, sizeof(ShareCode));

    conn_.setMessageCallback(std::bind(
        &ConnManager::getFileMsgHandle,
        this, _1, _2, _3, _4));
    conn_.send(buf, serveraddr_);
    state_ = WAIT_SENDER;
}

void ConnManager::chatMsg(Buffer &msg)
{
    Header header;
    header.sign = kClientSign;
    header.type = CHAT;
    header.dataLenth = msg.readableBytes();
    msg.prepend((char *)&header, sizeof(header));
    conn_.send(msg, recveraddr_);
}

void ConnManager::shareFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time)
{
    Header header = {0};
    Buffer sendMsg;
    ShareCode code;
    PeerAddr peerAddr = {0};
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
    case HELLO_REPLY:
        if ((state_ == CONN_RECVER) && sign == kClientSign)
        {
            if (0 != memcmp(&addr, &recveraddr_, sizeof(addr)))
            {
                LOG("Not expect peer.");
            }else
            {
                LOG("Connect peer success.");
                state_ = SEND_FILE;
            }
        }
        break;
    case SHARE_CODE:
        if (state_ != WAIT_CODE)
        {
            LOG("State %u error", state_);
            break;
        }
        memcpy(&code, buf.peek()+sizeof(Header), sizeof(ShareCode));
        code_ = code.code;
        LOG("Share code is %u", code_);
        client_->movePendConn(conn_.getFd(), code_);
        repeattimers_.push_back(
            conn_.getLoop()->runEvery(10, std::bind(
            &ConnManager::holdConn, this, serveraddr_, kServerSign)));
        state_ = WAIT_RECVER;
        break;
    case FILE_REQ:
        if (state_ != WAIT_RECVER)
        {
            LOG("State %u error", state_);
            break;
        }
        memcpy(&peerAddr, buf.peek()+sizeof(Header), sizeof(peerAddr));
        recveraddr_ = peerAddr.addr;

        header.sign = kClientSign;
        header.type = HELLO;
        header.dataLenth = 0;
        sendMsg.append((char *)&header, sizeof(Header));
        conn_.send(sendMsg, recveraddr_);
        repeattimers_.push_back(
            conn_.getLoop()->runEvery(10, std::bind(
            &ConnManager::holdConn, this, recveraddr_, kClientSign)));
        state_ = CONN_RECVER;
        break;
    case CHAT:
        LOG("Chat msg from %s:%s", ip_str, buf.peek()+sizeof(Header));
        break;
    case ERROR_FORMAT:
    case ERROR_LIMIT:
        closeSelf();
    default:
        break;
    }
}

void ConnManager::getFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time)
{
    Header header = {0};
    Buffer sendMsg;
    FileDataAddr fileDataAddr = {0};
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
    case HELLO_REPLY:
        if (state_ == CONN_SENDER && sign == kClientSign)
        {
            if (0 != memcmp(&addr, &recveraddr_, sizeof(addr)))
            {
                LOG("Not expect peer.");
            }else
            {
                LOG("Connect peer success.");
                state_ = RECV_FILE;
            }
        }
        break;
    case FILE_DATA:
        if (state_ != WAIT_SENDER)
        {
            LOG("State %u error", state_);
            break;
        }
        memcpy(&fileDataAddr, buf.peek()+sizeof(Header), sizeof(FileDataAddr));
        recveraddr_ = fileDataAddr.addr;
        LOG("Get file %s size:%lu bytes CRC32:%u",
            fileDataAddr.fileData.fileName,
            fileDataAddr.fileData.fileBytes,
            fileDataAddr.fileData.fileCRC32);
        recveraddr_ = fileDataAddr.addr;
        header.sign = kClientSign;
        header.type = HELLO;
        header.dataLenth = 0;
        sendMsg.append((char *)&header, sizeof(Header));
        conn_.send(sendMsg, recveraddr_);
        repeattimers_.push_back(
            conn_.getLoop()->runEvery(10, std::bind(
            &ConnManager::holdConn, this, recveraddr_, kClientSign)));
        state_ = CONN_SENDER;
        break;
    case CHAT:
        LOG("Chat msg from %s:%s", ip_str, buf.peek()+sizeof(Header));
        break;
    case ERROR_FORMAT:
    case ERROR_404:
        closeSelf();
    default:
        break;
    }
}

void ConnManager::holdConn(sockaddr &addr, uint8_t sign)
{
    Header header = {0};
    Buffer buf;
    header.sign = sign;
    header.type = HELLO;
    header.dataLenth = 0;
    buf.append((char *)&header, sizeof(header));
    conn_.send(buf, addr);
}

void ConnManager::closeSelf()
{
    if (code_ != 0)
    {
        client_->closeConn(code_);
    }
    else
    {
        client_->closePendConn(getFd());
    }
}
