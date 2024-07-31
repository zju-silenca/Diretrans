cmake_minimum_required (VERSION 3.2)

project (DirTrans)

option(ASAN "Enable or disable my option" OFF)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE Debug)

if(ASAN)
    # 启用 ASan
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

# 添加子目录
add_subdirectory(netlib)
add_subdirectory(server)
add_subdirectory(client)

