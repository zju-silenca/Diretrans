cmake_minimum_required (VERSION 3.2)

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

# 查找当前目录下的所有 .cpp 文件
file(GLOB SOURCES "*.cpp")

# 遍历所有 .cpp 文件
foreach(SOURCE_FILE ${SOURCES})
    # 获取文件名（不带扩展名）
    get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)
    
    # 添加可执行文件
    add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
    target_compile_options(${EXECUTABLE_NAME} PRIVATE -Wall)

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE pthread netlib)
    # 设置包含目录
    target_include_directories(${EXECUTABLE_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
endforeach()




