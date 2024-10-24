cmake_minimum_required(VERSION 3.5)

add_library(prf_union_find STATIC union_find.cpp)
target_include_directories(prf_union_find PUBLIC ./)
target_precompile_headers(prf_union_find PRIVATE union_find.hpp)

add_library(prf_node STATIC node.cpp)
target_include_directories(prf_node PUBLIC ./)
target_precompile_headers(prf_node PRIVATE node.hpp)
target_link_libraries(prf_node prf_union_find)

add_library(prf_rank STATIC rank.cpp)
target_include_directories(prf_rank PUBLIC ./)
target_precompile_headers(prf_rank PRIVATE rank.hpp)

add_library(prf STATIC prf.cpp)
target_include_directories(prf PUBLIC ./)
target_precompile_headers(prf PRIVATE prf.hpp)
target_link_libraries(prf prf_node prf_rank)
