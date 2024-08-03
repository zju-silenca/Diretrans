#ifndef _CRC32_H_
#define _CRC32_H_

#include <vector>
#include <iostream>

uint32_t calculateCRC32(const std::vector<char>& data);
uint32_t calculateFileCRC32(const std::string& filepath);

#endif
