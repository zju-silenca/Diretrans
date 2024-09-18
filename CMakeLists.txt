cmake_minimum_required (VERSION 3.2)

project (DirTrans)

option(ASAN "Enable or disable my option" OFF)
option(BUILD_TEST "Build test file" OFF)
option(DEBUG_BUILD "Build for Debug" ON)

set(CMAKE_CXX_STANDARD 17)
if(DEBUG_BUILD)
    message(STATUS "Build in Debug")
    set(CMAKE_BUILD_TYPE Debug)
else()
    message(STATUS "Build in Release")
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/bin)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/lib)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/lib)


message(STATUS "ASAN defined: " ${ASAN})
message(STATUS "BUILD_TEST defined: " ${BUILD_TEST})

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

if (BUILD_TEST)
    message(STATUS "Make test dir enabled")
    add_subdirectory(test)
else()
    message(STATUS "Make test dir disabled")
endif()

