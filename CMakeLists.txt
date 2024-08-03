cmake_minimum_required (VERSION 3.2)

project (DirTrans)

option(ASAN "Enable or disable my option" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

message(STATUS "ASAN defined: " ${ASAN})

if (ASAN)
    # 启用 ASan
    message(STATUS "AddressSanitizer enabled")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
else()
    message(STATUS "AddressSanitizer disabled")
endif()

# 添加子目录
add_subdirectory(netlib)
add_subdirectory(utils)
add_subdirectory(server)
add_subdirectory(client)

