#include "DiretransClient.h"
#include "ConnManager.h"

#include <random>
#include <string.h>
#include <arpa/inet.h>

using namespace std::placeholders;

static const uint32_t kPiecesPerWindow = 100;
static const uint32_t kWindowNum = 3;

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
    serveraddr_(client_->getServerAddr()),
    recvWindow_(0),
    sendWindow_(0),
    isWindowFull_(false)
{
    while (!conn_.isBound())
    {
        conn_.bindPort(getRandomPort());
    }
}

ConnManager::~ConnManager()
{

}

void ConnManager::shareFile(Buffer &fileName)
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
            std::bind(&ConnManager::holdConn, this, serveraddr_, kServerSign)));
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
            assert(*piecesPtr <= sendWindow_);
            sendWindow_ -= *piecesPtr;
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

void ConnManager::getFileMsgHandle(UdpCom *conn, Buffer &buf, sockaddr &addr, Timestamp &time)
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
                        std::bind(&ConnManager::sendRetranReq, this)));
                    conn_.send(sendMsg, recveraddr_);
                    state_ = RECV_FILE;
                }
                else
                {
                    closeSelf();
                }
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
            &ConnManager::holdConn, this, recveraddr_, kClientSign)));
        state_ = CONN_SENDER;
        break;
    case DATA_STREAM:
        if (state_ != RECV_FILE)
        {
            break;
        }
        assert(getManager_.get() && getManager_->isOpen());
        buf.retrieve(sizeof(Header));
        dataStream = (DataStream*)(buf.peek());
        buf.retrieve(sizeof(DataStream));
        ++recvWindow_;
        if (recvWindow_ >= kPiecesPerWindow)
        {
            header.sign = kClientSign;
            header.type = PACK_ACK;
            header.dataLenth = sizeof(PackageAck);
            sendMsg.append((char *)&header, sizeof(Header));
            sendMsg.append((char *)&recvWindow_, sizeof(uint32_t));
            recvWindow_ = 0;
            conn_.send(sendMsg, recveraddr_);
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

void ConnManager::sendFileStream()
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
    client_->getLoop()->queueInLoop(std::bind(&ConnManager::sendFileStream, this));
}

void ConnManager::sendLossPieces(std::vector<uint32_t> pieces)
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
    conn_.getLoop()->queueInLoop(std::bind(&ConnManager::sendLossPieces, this, pieces));
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

void ConnManager::sendRetranReq()
{
    std::vector<uint32_t> peices;
    Header header = {0};
    RetransPieces retrans = {0};
    Buffer sendMsg;
    if (state_ != RECV_FILE || getManager_.get() == nullptr)
    {
        return;
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

void ConnManager::closeAllTimer()
{
    repeattimers_.clear();
}
