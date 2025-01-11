cmake_minimum_required(VERSION 3.5)

file(GLOB prf_source "prf/*.cpp")
file(GLOB prf_header "prf/*.hpp")

add_library(prf STATIC ${prf_source})
target_include_directories(prf PUBLIC ./)
target_link_libraries(prf PUBLIC "-pthread")
set_property(TARGET prf PROPERTY POSITION_INDEPENDENT_CODE ON)
