#include "DiretransClient.h"
#include "SendConnManager.h"

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

SendConnManager::SendConnManager(DiretransClient *client)
    : client_(client),
    conn_(client_->getLoop(), getRandomPort()),
    state_(IDLE),
    code_(0),
    serveraddr_(client_->getServerAddr()),
    sendWindow_(0),
    isWindowFull_(false)
{
    while (!conn_.isBound())
    {
        conn_.bindPort(getRandomPort());
    }
}

SendConnManager::~SendConnManager()
{

}

void SendConnManager::shareFile(Buffer &fileName)
{
    Buffer buf;
    Header header = {0};
    FileData fileData = {0};
    std::string strFileName;

    assert(state_ == IDLE);
    assert(conn_.isBound());

    strFileName = fileName.retrieveAsString();
    sendManager_ = std::make_unique<SendFileManager>(strFileName, kBytesPerPiece);
    if (!sendManager_->isOpen())
    {
        closeSelf();
        return;
    }
    strncpy(fileData.fileName, sendManager_->getFileName().c_str(), sendManager_->getFileName().size());
    fileData.fileBytes = sendManager_->getFileBytes();
    fileData.fileCRC32 = sendManager_->getFileCRC32();
    header.sign = kServerSign;
    header.type = SHARE_FILE;
    header.dataLenth = sizeof(FileData);

    buf.append((char *)&header, sizeof(Header));
    buf.append((char *)&fileData, sizeof(FileData));

    conn_.setMessageCallback(std::bind(
        &SendConnManager::shareFileMsgHandle,
        this, _1, _2, _3, _4));
    conn_.send(buf, serveraddr_);
    state_ = WAIT_CODE;
}

void SendConnManager::chatMsg(Buffer &msg)
{
    Header header;
    header.sign = kClientSign;
    header.type = CHAT;
    header.dataLenth = msg.readableBytes();
    msg.prepend((char *)&header, sizeof(header));
    conn_.send(msg, recveraddr_);
}

void SendConnManager::shareFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time)
{
    Header header = {0};
    static Buffer sendMsg; // 会被频繁调用，设为static防止资源反复释放
    ShareCode code = {0};
    PeerAddr peerAddr = {0};
    uint8_t sign = 0;
    RetransPieces retranPieces = {0};
    uint32_t* piecesPtr = nullptr;
    std::vector<uint32_t> pieces;
    size_t i = 0;

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
        if (0 == memcmp(&addr, &serveraddr_, sizeof(addr)))
        {
            lastServerHelloTime_ = time;
        }else if (0 == memcmp(&addr, &recveraddr_, sizeof(addr)))
        {
            lastPeerHelloTime_ = time;
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
        heartBeatTimerId_ = std::move(conn_.getLoop()->runEvery(10,
            std::bind(&SendConnManager::holdConn, this, serveraddr_, kServerSign)));
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
            &SendConnManager::holdConn, this, recveraddr_, kClientSign)));
        state_ = CONN_RECVER;
        break;
    case DOWNLOAD_START:
        if (state_ == SEND_FILE || state_ == CONN_RECVER)
        {
            state_ = SEND_FILE;
            sendFileStream();
        }
        else
        {
            LOG("state error: %u", state_);
        }
        break;
    case PACK_ACK:
        if (state_ == SEND_FILE)
        {
            piecesPtr = (uint32_t*)(buf.peek() + sizeof(Header));
            LOG("Recver ack %u pieces. Window is: %u", *piecesPtr, sendWindow_);
            if (*piecesPtr <= sendWindow_)
            {
                sendWindow_ -= *piecesPtr;
            }else
            {
                sendWindow_ = 0;
            }
            if (isWindowFull_)
            {
                isWindowFull_ = false;
                sendFileStream();
            }
        }
        break;
    case RETRAN:
        if (state_ == SEND_FILE)
        {
            memcpy(&retranPieces, buf.peek() + sizeof(Header), sizeof(RetransPieces));
            LOG("Need to resend %u pieces data.", retranPieces.nums);
            pieces.resize(retranPieces.nums);
            piecesPtr = (uint32_t *)(buf.peek() + sizeof(Header) + sizeof(RetransPieces));
            for (i = 0; i < retranPieces.nums; ++i)
            {
                pieces.push_back(*piecesPtr);
                ++piecesPtr;
            }
            sendLossPieces(pieces);
        }
        break;
    case FINISH:
        if (state_ == SEND_FILE)
        {
            state_ = WAIT_RECVER;
            sendWindow_ = 0;
            isWindowFull_ = false;
            lastPeerHelloTime_ = Timestamp::invalid();
            sendManager_->initStatu();
            closeAllTimer();
        }
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

void SendConnManager::holdConn(sockaddr &addr, uint8_t sign)
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

void SendConnManager::sendFileStream()
{
    Header header = {0};
    DataStream dataStream = {0};
    static Buffer sendMsg;

    sendMsg.retrieveAll();
    if (!sendManager_->isGood() || sendWindow_ >= kPiecesPerWindow * kWindowNum)
    {
        isWindowFull_ = true;
        return;
    }
    dataStream.pieceNum = sendManager_->getCurPieceNum();
    Buffer buf = sendManager_->getNextPiece();
    header.sign = kClientSign;
    header.type = DATA_STREAM;
    header.dataLenth = buf.readableBytes() + sizeof(DataStream);
    sendMsg.append((char *)&header, sizeof(Header));
    sendMsg.append((char *)&dataStream, sizeof(DataStream));
    sendMsg.append(buf.peek(), buf.readableBytes());
    conn_.send(sendMsg, recveraddr_);
    ++sendWindow_;
    client_->getLoop()->queueInLoop(std::bind(&SendConnManager::sendFileStream, this));
}

void SendConnManager::sendLossPieces(std::vector<uint32_t> pieces)
{
    Header header = {0};
    DataStream dataStream = {0};
    static Buffer sendMsg;

    sendMsg.retrieveAll();
    if (!sendManager_->isOpen() || pieces.size() == 0 ||
     sendWindow_ >= kPiecesPerWindow * kWindowNum)
    {
        isWindowFull_ = true;
        return;
    }
    dataStream.pieceNum = pieces[pieces.size() - 1];
    pieces.pop_back();
    Buffer buf = sendManager_->getPieceOf(dataStream.pieceNum);
    header.sign = kClientSign;
    header.type = DATA_STREAM;
    header.dataLenth = buf.readableBytes() + sizeof(DataStream);
    sendMsg.append((char *)&header, sizeof(Header));
    sendMsg.append((char *)&dataStream, sizeof(DataStream));
    sendMsg.append(buf.peek(), buf.readableBytes());
    conn_.send(sendMsg, recveraddr_);
    ++sendWindow_;
    conn_.getLoop()->queueInLoop(std::bind(&SendConnManager::sendLossPieces, this, pieces));
}

void SendConnManager::closeSelf()
{
    if (code_ != 0)
    {
        client_->closeSendConn(code_);
    }
    else
    {
        client_->closePendConn(getFd());
    }
}

void SendConnManager::checkPeerAlive()
{
    if (state_ == SEND_FILE && lastPeerHelloTime_.valid() &&
        lastPeerHelloTime_.getMicroSeconds() + kTimeoutSec * Timestamp::kMicroSecondsPerSecond < Timestamp::now().getMicroSeconds())
    {
        LOG("Peer may dead.");
        state_ = WAIT_RECVER;
        sendWindow_ = 0;
        isWindowFull_ = false;
        sendManager_->initStatu();
        closeAllTimer();
    }

    if (lastServerHelloTime_.valid() &&
        lastServerHelloTime_.getMicroSeconds() + kTimeoutSec * Timestamp::kMicroSecondsPerSecond < Timestamp::now().getMicroSeconds())
    {
        LOG("Server offline.");
        closeSelf();
    }
}

void SendConnManager::closeAllTimer()
{
    repeattimers_.clear();
}
