
# # 获取项目根目录的绝对路径
# get_filename_component(PROJECT_SOURCE_DIR_ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# 遍历当前文件夹中的所有源文件
file(GLOB_RECURSE ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

# # 过滤掉 CMake 内部生成的文件
# list(FILTER ALL_SOURCES EXCLUDE REGEX "${CMAKE_BINARY_DIR}/")

# foreach(source IN LISTS ALL_SOURCES)
#     # 获取源文件的相对路径
#     file(RELATIVE_PATH source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
#     # 设置编译选项，定义 `__RELATIVE_FILE__` 为相对路径
#     set_source_files_properties(${source} PROPERTIES COMPILE_FLAGS "-D__RELATIVE_FILE__='\"${source_rel}\"'")
# endforeach()

add_library(netlib STATIC ${ALL_SOURCES})

target_compile_options(netlib PRIVATE -Wall)

target_include_directories(netlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
