cmake_minimum_required(VERSION 3.10)
project(Rchess)

add_compile_options(-pg)

add_library(libRchess
    src/libRchess/board.cpp
    src/libRchess/move.cpp
    src/libRchess/state.cpp
    src/libRchess/magic.cpp
)

add_executable(Rchess src/Rchess.cpp)
target_link_libraries(Rchess libRchess)
