#ifndef _CONNMANAGER_H_
#define _CONNMANAGER_H_

static const uint32_t kPiecesPerWindow = 10;
static const uint32_t kWindowNum = 3;
// 对端持续60s未返回HELLO_REPLY认为连接丢失
static const uint32_t kTimeoutSec = 60;
// 超过100ms未接收到文件数据，重发PACK_ACK，清空对端发送窗口
static const int64_t kFileStreamTimeoutUS = 100 * 1000;

class ConnManager
{
public:
    virtual ~ConnManager() = default; // 确保析构函数是虚函数
private:

};

#endif