cmake_minimum_required(VERSION 3.10)
project(chest)

add_compile_options(-O3 -flto -Wall)

add_library(libChest
    src/libChest/board.cpp
    src/libChest/move.cpp
    src/libChest/state.cpp
    src/libChest/magic.cpp
)

add_executable(chest src/chest.cpp)
target_link_libraries(chest libChest)
