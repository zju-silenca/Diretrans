#include "SendFileManager.h"
#include "netlib/Buffer.h"
#include "utils/CRC32.h"
#include "netlib/Log.h"

#include <filesystem>

SendFileManager::SendFileManager(std::string& filePath, uint32_t bytesPerpiece)
    : bytesPerPiece_(bytesPerpiece)
{
    std::filesystem::path path(filePath);
    if (!std::filesystem::exists(path))
    {
        LOG("File %s not exist.", filePath.c_str());
        return;
    }
    else
    {
        openFile(filePath);
    }
}

SendFileManager::~SendFileManager()
{
    if (isOpen())
    {
        ifileStream_.close();
    }
}

void SendFileManager::openFile(std::string filePath)
{
    std::filesystem::path path(filePath);
    ifileStream_.open(filePath, std::ifstream::binary);
    if (!ifileStream_.is_open())
    {
        LOG("Open file %s failed.", filePath.c_str());
        return;
    }
    filePath_ = filePath;
    fileName_ = path.filename().string();
    fileBytes_ = std::filesystem::file_size(path);
    fileCRC32_ = calculateFileCRC32(filePath);
    curPiece_ = 0;
    LOG("Open file %s success.\n"
        "File size: %lu bytes.\n"
        "File CRC32: %0X", filePath.c_str(), fileBytes_, fileCRC32_);
    return;
}

void SendFileManager::initStatu()
{
    if (!isGood())
    {
        ifileStream_.clear();
    }
    ifileStream_.seekg(std::ios::beg);
    curPiece_ = 0;
}

Buffer SendFileManager::getNextPiece()
{
    Buffer res;
    assert(isOpen());
    if (!ifileStream_.good())
    {
        LOG("File %s stream is not good.", fileName_.c_str());
        return res;
    }

    res.ensureWritableBytes(bytesPerPiece_);
    ifileStream_.read(res.beginWrite(), bytesPerPiece_);
    auto readBytes = ifileStream_.gcount();
    if (readBytes == 0)
    {
        return res;
    }
    if (readBytes != bytesPerPiece_)
    {
        assert(!ifileStream_.good());
    }
    res.hasWritten(readBytes);
    ++curPiece_;
    return res;
}

Buffer SendFileManager::getPieceOf(uint32_t num)
{
    assert(isOpen());
    uint32_t curPiece = curPiece_;
    if (!isGood())
    {
        ifileStream_.clear();
    }
    ifileStream_.seekg(num * bytesPerPiece_, std::ios::beg);
    Buffer res = std::move(getNextPiece());
    curPiece_ = curPiece;
    ifileStream_.seekg(curPiece_ * bytesPerPiece_, std::ios::beg);
    return res;
}
