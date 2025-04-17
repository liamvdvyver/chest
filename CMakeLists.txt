cmake_minimum_required(VERSION 3.10)
project(chest)

add_compile_options(-std=c++20 -O3 -flto=auto -Wall -Wextra)

add_library(libChest
    src/libChest/board.cpp
    src/libChest/state.cpp
)

add_executable(chest src/chest.cpp)
target_link_libraries(chest libChest)
