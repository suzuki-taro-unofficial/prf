cmake_minimum_required(VERSION 3.5)

file(GLOB prf_source "*.cpp")
file(GLOB prf_header "*.hpp")

add_library(prf STATIC ${prf_source})
target_include_directories(prf PUBLIC ./)
