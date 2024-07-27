cmake_minimum_required (VERSION 3.2)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BUILD_TYPE Debug)

# 获取项目根目录的绝对路径
get_filename_component(PROJECT_SOURCE_DIR_ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# 遍历项目中的所有源文件
file(GLOB_RECURSE ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

# 过滤掉 CMake 内部生成的文件
list(FILTER ALL_SOURCES EXCLUDE REGEX "${CMAKE_BINARY_DIR}/")

foreach(source IN LISTS ALL_SOURCES)
    # 获取源文件的相对路径
    file(RELATIVE_PATH source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
    # 设置编译选项，定义 `__RELATIVE_FILE__` 为相对路径
    set_source_files_properties(${source} PROPERTIES COMPILE_FLAGS "-D__RELATIVE_FILE__='\"${source_rel}\"'")
endforeach()

add_executable(server ${ALL_SOURCES})

target_compile_options(server PRIVATE -Wall)

target_link_libraries(server PRIVATE pthread netlib)
# 设置包含目录
target_include_directories(server PUBLIC ${CMAKE_SOURCE_DIR} "../utils")


