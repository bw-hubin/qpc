# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_LIB_SRCS 变量
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} DIR_LIB_SRCS)


# 排除指定文件
list (FILTER DIR_LIB_SRCS EXCLUDE REGEX "server")


set(SRCS_COMM
  ${DIR_LIB_SRCS}
  PARENT_SCOPE
)
