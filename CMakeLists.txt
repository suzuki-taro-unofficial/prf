cmake_minimum_required(VERSION 3.5)
project(prf)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# デバッグするときに有効化すると良い
# add_compile_options(-fsanitize=address -g -Wstack-protector)
# add_link_options(-fsanitize=address -g -Wstack-protector)

add_subdirectory(src)
add_subdirectory(test)
