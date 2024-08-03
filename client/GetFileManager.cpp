#include "GetFileManager.h"
#include "netlib/Buffer.h"
#include "utils/CRC32.h"
#include "netlib/Log.h"

#include <filesystem>

namespace fs = std::filesystem;

static const uint32_t kMaxLossPiecesNum = 1024;

GetFileManager::GetFileManager(FileData &data, uint32_t bytesPerPiece)
    : curPiece_(0),
    fileName_(data.fileName),
    fileBytes_(data.fileBytes),
    fileCRC32_(data.fileCRC32),
    bytesPerPiece_(bytesPerPiece),
    writtenBytes_(0)
{
    fs::path curPath = fs::current_path();
    fs::path filePath = curPath/fileName_;

    if (fs::exists(filePath))
    {
        LOG("File %s exist in cur path.", fileName_.c_str());
        return;
    }

    if (fileBytes_ > UINT32_MAX * bytesPerPiece)
    {
        LOG("File too large.");
        return;
    }

    fileStream_.open(fileName_, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    if (!fileStream_.is_open())
    {
        LOG("Create file error. errno:%d", errno);
    }
    filePath_ = filePath.string();
}

GetFileManager::~GetFileManager()
{
    if (isOpen())
    {
        fileStream_.close();
    }
}

WriteState GetFileManager::writeToFile(Buffer &data, uint32_t piece)
{
    WriteState ret = NONE;
    char zeroData[bytesPerPiece_] = {0};
    if (!isOpen())
    {
        LOG("Write file stream not open.");
        return NOT_OPEN;
    }

    if (piece > curPiece_)
    {
        if (piece > curPiece_ + kMaxLossPiecesNum)
        {
            LOG("Loss too much pieces: %u", piece - curPiece_);
            return LOSS_HEAVY;
        }

        for (size_t i = curPiece_; i < piece; ++i)
        {
            fileStream_.write(zeroData, bytesPerPiece_);
            lossPieces_.insert(i);
        }
        fileStream_.write(data.peek(), data.readableBytes());
        writtenBytes_ += data.readableBytes();
        curPiece_ = piece + 1;
        ret = LOSS;
    } else if (piece < curPiece_)
    {
        auto it = lossPieces_.find(piece);
        if (it != lossPieces_.end())
        {
            fileStream_.seekp(piece * bytesPerPiece_, std::ios::beg);
            fileStream_.write(data.peek(), data.readableBytes());
            writtenBytes_ += data.readableBytes();
            fileStream_.seekp(curPiece_ * bytesPerPiece_, std::ios::beg);
            lossPieces_.erase(it);
        }
        ret = NONE;
    } else // piece == curPiece_
    {
        fileStream_.write(data.peek(), data.readableBytes());
        writtenBytes_ += data.readableBytes();
        ++curPiece_;
        ret = NONE;
    }

    LOG("writtenBytes_: %u / %u, %.2f\%", writtenBytes_, fileBytes_, (1.0 * writtenBytes_ / fileBytes_) * 100.0);
    if (writtenBytes_ == fileBytes_)
    {
        //assert(lossPieces_.size() == 0);
        if (lossPieces_.size() != 0)
        {
            for (auto piece : lossPieces_)
            {
                LOG("Loss piece: %u", piece);
            }
        }
        fileStream_.flush();
        fileStream_.close();
        checkCRC32();
        ret = WRITE_FINISH;
    }else if (writtenBytes_ > fileBytes_)
    {
        ret = RECV_ERROR;
    }

    return ret;
}

void GetFileManager::getLossPieces(std::vector<uint32_t> &buf)
{
    for (auto piece : lossPieces_)
    {
        buf.push_back(piece);
    }
}

void GetFileManager::checkCRC32()
{
    uint32_t recvCRC = calculateFileCRC32(filePath_);

    if (recvCRC != fileCRC32_)
    {
        LOG("Recv crc:%0X is diff with %0X", recvCRC, fileCRC32_);
    }else
    {
        LOG("Recv crc:%0X check success.", recvCRC);
    }
}
