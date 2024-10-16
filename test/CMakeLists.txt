cmake_minimum_required(VERSION 3.1)

add_executable(hello_test hello_test.cpp)

target_link_libraries(hello_test hello)

add_test(NAME run_test COMMAND hello_test)
