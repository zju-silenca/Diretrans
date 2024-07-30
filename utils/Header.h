#ifndef _HEADER_H_
#define _HEADER_H_

#include <stdint.h>
#include <sys/socket.h>

const static uint8_t kServerSign = 0xCCu;
const static uint8_t kClientSign = 0xDDu;
const static uint8_t kFileNameLenth = 0xFFu;
const static uint8_t kShareCodeLen = 6;
const static uint8_t kPasswordLen = 12;

typedef uint32_t sharecode;

typedef enum MessageType_ : uint8_t
{
    HELLO = 0,
    HELLO_REPLY,
    SHARE_FILE,
    SHARE_CODE,
    GET_FILE,
    FILE_DATA,
    FILE_REQ, // 有人请求分享码，通知分享者
    DOWNLOAD_START, // 开始下载
    DATA_STREAM, // 文件数据流
    RETRAN, // 重传请求
    FINISH, // 传输完成
    ERROR_FORMAT = 0xf0u, // 格式错误
    ERROR_LIMIT, // 分享达到上限
    ERROR_404, // 分享码不存在
    TYPE_MAX
} MessageType;

#pragma pack(1)

typedef struct Header_
{
    uint8_t sign;
    MessageType type;
    uint16_t dataLenth;
} Header;

typedef struct ShareCode_
{
    sharecode code;
} ShareCode;


typedef struct FileData_
{
    char fileName[kFileNameLenth + 1];
    uint64_t fileBytes;
    uint32_t fileCRC32;
} FileData;

typedef struct FileDataAddr_
{
    FileData fileData;
    struct sockaddr addr;
} FileDataAddr;

typedef struct PeerAddr_
{
    struct sockaddr addr;
} PeerAddr;

#pragma pack()
#endif