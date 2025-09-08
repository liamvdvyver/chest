cmake_minimum_required(VERSION 3.10)
project(chest)

message("Compiler Version: ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fomit-frame-pointer -flto -ffast-math -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g -fsanitize=address,undefined" CACHE STRING "" FORCE)

add_compile_options(-std=c++23 -Wall -Wextra -Wpedantic)
add_link_options(-fuse-ld=lld)

# Main

add_library(libChest
    libChest/libChest/board.cpp
    libChest/libChest/state.cpp
)
target_include_directories(libChest PUBLIC libChest)

file(GLOB CHEST
    chest/chest/*.h
    chest/chest/*.cpp
)
add_executable(chest ${CHEST})
target_link_libraries(chest libChest)

# Formatting

file(GLOB_RECURSE ALL_SOURCE_FILES *.c *.h)
add_custom_target(
    format_check
    ALL
    COMMAND /usr/bin/clang-format
    --dry-run
    -Werror
    ${ALL_SOURCE_FILES}
)

# Linting

add_custom_target(
    lint
    COMMAND /usr/bin/clang-tidy
    ${ALL_SOURCE_FILES}
)

# Tests

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.3
)
FetchContent_MakeAvailable(Catch2)

add_executable(makemove_test libChest/libChest/makemove.test.cpp)
target_link_libraries(makemove_test PRIVATE libChest Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(makemove_test)
