cmake_minimum_required(VERSION 3.5)

add_executable(rank_test rank_test.cpp)
target_link_libraries(rank_test prf)
add_test(run_rank_test rank_test)

add_executable(node_test node_test.cpp)
target_link_libraries(node_test prf)
add_test(run_node_test node_test)

add_executable(concurrent_queue_test concurrent_queue_test.cpp)
target_link_libraries(concurrent_queue_test prf)
add_test(run_concurrent_queue_test concurrent_queue_test)
