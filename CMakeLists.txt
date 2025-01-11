cmake_minimum_required(VERSION 3.5)
project(prf)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# INFO系列のログを表示するか否か
set(PRF_SHOW_INFO_LOG OFF CACHE BOOL "show info log from prf")
if(PRF_SHOW_INFO_LOG)
  add_compile_options(-DSHOW_INFO_LOG)
  add_link_options(-DSHOW_INFO_LOG)
endif()

# WARN系列のログを表示するか否か
set(PRF_WARN_INFO_LOG OFF CACHE BOOL "show warn log from prf")
if(PRF_SHOW_WARN_LOG)
  add_compile_options(-DSHOW_WARN_LOG)
  add_link_options(-DSHOW_WARN_LOG)
endif()

set(PRF_DEBUG OFF CACHE BOOL "add compiler flags to debugging")
if(PRF_DEBUG)
  # デバッグするときに有効化すると良い
  add_compile_options(-fsanitize=address -g -Wstack-protector)
  add_link_options(-fsanitize=address -g -Wstack-protector)
endif()

add_subdirectory(src)
add_subdirectory(test)
