#ifndef _SENDFILEMANAGER_H_
#define _SENDFILEMANAGER_H_

#include <fstream>
#include <string>

class Buffer;

/*
***********NOT THREAD SAFE**********
*/
class SendFileManager
{
public:
    SendFileManager() = delete;
    SendFileManager(std::string& filePath, uint32_t bytesPerpiece);
    SendFileManager(const SendFileManager &) = delete;
    ~SendFileManager();

    void openFile(std::string filePath);
    bool isOpen() const { return ifileStream_.is_open(); }
    bool isGood() const { return ifileStream_.good(); }
    std::string getFileName() const { return fileName_; }
    std::string getFilePath() const { return filePath_; }
    uint32_t getFileCRC32() const { return fileCRC32_; }
    uint32_t getCurPieceNum() const { return curPiece_; }
    uint64_t getFileBytes() const { return fileBytes_; }
    Buffer getNextPiece();
    Buffer getPieceOf(uint32_t num);

private:
    std::ifstream ifileStream_;
    uint32_t curPiece_;
    std::string filePath_;
    std::string fileName_;
    uint64_t fileBytes_;
    uint32_t fileCRC32_;
    const uint32_t bytesPerPiece_;
};

#endif