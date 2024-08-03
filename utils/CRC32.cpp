#include "CRC32.h"

#include <fstream>
#include <iomanip>


// 预计算的 CRC32 查找表
static uint32_t crcTable[256];
static bool isInit = false;

// 初始化 CRC32 查找表
static void generateCRCTable() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (uint32_t j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crcTable[i] = crc;
    }
    isInit = true;
}

// 计算缓冲区的 CRC32
uint32_t calculateCRC32(const std::vector<char>& data) {
    if (!isInit)
    {
        generateCRCTable();
    }
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < data.size(); ++i) {
        uint8_t byte = data[i];
        crc = (crc >> 8) ^ crcTable[(crc ^ byte) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

// 计算文件的 CRC32
uint32_t calculateFileCRC32(const std::string& filepath) {
    if (!isInit)
    {
        generateCRCTable();
    }
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "无法打开文件" << std::endl;
        return 0;
    }

    const size_t bufferSize = 1024;
    std::vector<char> buffer(bufferSize);

    uint32_t crc = 0xFFFFFFFF;  // 初始化 CRC32

    while (file.read(buffer.data(), buffer.size())) {
        for (int i = 0; i < file.gcount(); ++i) {
            uint8_t byte = buffer[i];
            crc = (crc >> 8) ^ crcTable[(crc ^ byte) & 0xFF];
        }
    }

    // 处理最后读取的部分数据
    if (file.gcount() > 0) {
        for (int i = 0; i < file.gcount(); ++i) {
            uint8_t byte = buffer[i];
            crc = (crc >> 8) ^ crcTable[(crc ^ byte) & 0xFF];
        }
    }

    file.close();
    return crc ^ 0xFFFFFFFF;
}
