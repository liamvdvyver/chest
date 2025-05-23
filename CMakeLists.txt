cmake_minimum_required(VERSION 3.10)
project(chest)

add_compile_options(-std=c++23 -Ofast -flto=auto -march=native -fomit-frame-pointer -Wall -Wextra -Wpedantic -DNDEBUG)
add_link_options()

# Main

add_library(libChest
    src/libChest/libChest/board.cpp
    src/libChest/libChest/state.cpp
)
target_include_directories(libChest PUBLIC src/libChest)

add_executable(chest src/chest/chest/chest.cpp)
target_link_libraries(chest libChest)

# Tests

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.3 # or the latest stable
)
FetchContent_MakeAvailable(Catch2)

add_executable(makemove_test src/libChest/libChest/makemove.test.cpp)
target_link_libraries(makemove_test PRIVATE libChest Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(makemove_test)
