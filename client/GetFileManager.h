#ifndef _GETFILEMANAGER_H_
#define _GETFILEMANAGER_H_

#include "utils/Header.h"
#include "utils/CRC32.h"
#include <fstream>
#include <string>
#include <set>

class Buffer;

typedef enum WriteState_ : uint8_t
{
    NONE = 0,
    LOSS,
    LOSS_HEAVY, // 接收到的piece与当前peice差别过大
    WRITE_FINISH,
    NOT_OPEN,
    RECV_ERROR,
} WriteState;

class GetFileManager
{
public:
    GetFileManager(FileData& data, uint32_t bytesPerPiece);
    ~GetFileManager();

    WriteState writeToFile(Buffer& data, uint32_t piece);
    void getLossPieces(std::vector<uint32_t>& buf);

    bool isOpen() { return fileStream_.is_open(); }

private:
    void checkCRC32();

    std::fstream fileStream_;
    std::set<uint32_t> lossPieces_;
    uint32_t curPiece_;
    std::string filePath_;
    const std::string fileName_;
    const uint64_t fileBytes_;
    const uint32_t fileCRC32_;
    const uint32_t bytesPerPiece_;
    uint64_t writtenBytes_;
};

#endif