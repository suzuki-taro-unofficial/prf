cmake_minimum_required(VERSION 3.1)

add_library(hello STATIC hello.cpp)

target_include_directories(hello PUBLIC ./)
