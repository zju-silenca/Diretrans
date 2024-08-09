#include "DiretransClient.h"
#include "GetConnManager.h"

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

GetConnManager::GetConnManager(DiretransClient *client)
    : client_(client),
    conn_(client_->getLoop(), getRandomPort()),
    state_(IDLE),
    code_(0),
    serveraddr_(client_->getServerAddr()),
    recvWindow_(0),
    isWindowFull_(false)
{
    while (!conn_.isBound())
    {
        conn_.bindPort(getRandomPort());
    }
}

GetConnManager::~GetConnManager()
{

}

void GetConnManager::getFile(sharecode code)
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
        &GetConnManager::getFileMsgHandle,
        this, _1, _2, _3, _4));
    conn_.send(buf, serveraddr_);
    state_ = WAIT_SENDER;
}

void GetConnManager::chatMsg(Buffer &msg)
{
    Header header;
    header.sign = kClientSign;
    header.type = CHAT;
    header.dataLenth = msg.readableBytes();
    msg.prepend((char *)&header, sizeof(header));
    conn_.send(msg, recveraddr_);
}

void GetConnManager::getFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time)
{
    Header header = {0};
    static Buffer sendMsg;
    FileDataAddr fileDataAddr = {0};
    uint8_t sign = 0;
    DataStream* dataStream = nullptr;

    sendMsg.retrieveAll();
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
                header.sign = kClientSign;
                header.type = DOWNLOAD_START;
                header.dataLenth = 0;
                sendMsg.append((char *)&header, sizeof(header));
                getManager_ = std::make_unique<GetFileManager>(fileData_, kBytesPerPiece);
                if (getManager_->isOpen())
                {
                    repeattimers_.push_back(conn_.getLoop()->runEvery(1.0,
                        std::bind(&GetConnManager::sendRetranReq, this)));
                    conn_.send(sendMsg, recveraddr_);
                    state_ = RECV_FILE;
                }
                else
                {
                    closeSelf();
                }
            }
        }
        if (0 == memcmp(&addr, &serveraddr_, sizeof(addr)))
        {
            lastServerHelloTime_ = time;
        }else if (0 == memcmp(&addr, &recveraddr_, sizeof(addr)))
        {
            lastPeerHelloTime_ = time;
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
        fileData_ = fileDataAddr.fileData;
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
            &GetConnManager::holdConn, this, recveraddr_, kClientSign)));
        state_ = CONN_SENDER;
        break;
    case DATA_STREAM:
        if (state_ != RECV_FILE)
        {
            break;
        }
        lastDataStreamTime_ = time;
        assert(getManager_.get() && getManager_->isOpen());
        buf.retrieve(sizeof(Header));
        dataStream = (DataStream*)(buf.peek());
        buf.retrieve(sizeof(DataStream));
        ++recvWindow_;
        if (recvWindow_ >= kPiecesPerWindow)
        {
            sendPackAck(recvWindow_);
            recvWindow_ = 0;
        }
        if (WRITE_FINISH == getManager_->writeToFile(buf, dataStream->pieceNum))
        {
            state_ = IDLE;
            header.sign = kClientSign;
            header.type = FINISH;
            header.dataLenth = 0;
            sendMsg.append((char *)&header, sizeof(Header));
            conn_.send(sendMsg, recveraddr_);
            closeSelf();
        }
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

void GetConnManager::holdConn(sockaddr &addr, uint8_t sign)
{
    Header header = {0};
    Buffer buf;
    header.sign = sign;
    header.type = HELLO;
    header.dataLenth = 0;
    buf.append((char *)&header, sizeof(header));
    conn_.send(buf, addr);
    checkPeerAlive();
}

void GetConnManager::closeSelf()
{
    if (code_ != 0)
    {
        client_->closeGetConn(code_);
    }
}

void GetConnManager::sendRetranReq()
{
    std::vector<uint32_t> peices;
    Header header = {0};
    RetransPieces retrans = {0};
    Buffer sendMsg;
    if (state_ != RECV_FILE || getManager_.get() == nullptr)
    {
        return;
    }

    if (lastDataStreamTime_.getMicroSeconds() + kFileStreamTimeoutUS < Timestamp::now().getMicroSeconds())
    {
        LOG("Get file timeout, send PACK_ACK.");
        sendPackAck(kWindowNum * kPiecesPerWindow);
    }

    getManager_->getLossPieces(peices);
    if (peices.size() == 0)
    {
        return;
    }
    retrans.nums = peices.size();

    header.sign = kClientSign;
    header.type = RETRAN;
    header.dataLenth = retrans.nums * sizeof(uint32_t) + sizeof(RetransPieces);
    sendMsg.append((char *)&header, sizeof(header));
    sendMsg.append((char *)&retrans, sizeof(retrans));
    sendMsg.append((char *)&peices[0], retrans.nums * sizeof(uint32_t));
    conn_.send(sendMsg, recveraddr_);
}

void GetConnManager::sendPackAck(uint32_t nums)
{
    Buffer sendMsg;
    Header header = {0};
    header.sign = kClientSign;
    header.type = PACK_ACK;
    header.dataLenth = sizeof(PackageAck);
    sendMsg.append((char *)&header, sizeof(Header));
    sendMsg.append((char *)&nums, sizeof(uint32_t));
    conn_.send(sendMsg, recveraddr_);
}

void GetConnManager::checkPeerAlive()
{
    if (lastPeerHelloTime_.valid() &&
        lastPeerHelloTime_.getMicroSeconds() + kTimeoutSec * Timestamp::kMicroSecondsPerSecond < Timestamp::now().getMicroSeconds())
    {
        LOG("Peer may dead.");
        closeSelf();
    }
}

void GetConnManager::closeAllTimer()
{
    repeattimers_.clear();
}
