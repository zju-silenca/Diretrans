#ifndef _LOH_H_
#define _LOG_H_

#include "Timestamp.h"

#include <iostream>
#include <iomanip>

#ifndef __RELATIVE_FILE__
#define __RELATIVE_FILE__ __FILE__
#endif


#define LOG(format, ...) \
    do { \
        fprintf(stderr, "[%s] [%s:%d] " format "\n", Timestamp::now().toFormateString().c_str(), __RELATIVE_FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#endif