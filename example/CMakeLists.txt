cmake_minimum_required(VERSION 3.5)


add_executable(pascal_triangle pascal-triangle.cpp)
target_link_libraries(pascal_triangle prf)
target_include_directories(pascal_triangle PUBLIC ./)
