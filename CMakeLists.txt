cmake_minimum_required(VERSION 3.22)
project(glynth VERSION 0.0.1 LANGUAGES C CXX)

# Force C++20 for sanity
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG VER-2-14-1)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 12.1.0)
FetchContent_MakeAvailable(freetype fmt)

add_executable(read src/read.cpp)
target_link_libraries(read PRIVATE freetype fmt::fmt)