cmake_minimum_required(VERSION 3.22)
project(glynth VERSION 0.0.1 LANGUAGES C CXX)

# Force C++20 for sanity
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG VER-2-14-1
)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 12.1.0
)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.2
)
FetchContent_Declare(
    libnpy
    GIT_REPOSITORY https://github.com/matajoh/libnpy.git
    GIT_TAG v1.5.3
)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
)
FetchContent_MakeAvailable(
    freetype
    fmt
    glm
    libnpy
    juce
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Gperftools COMPONENTS PROFILER)

add_executable(read src/read.cpp src/outliner.cpp)
target_link_libraries(
    read
    PRIVATE freetype fmt::fmt glm::glm npy::npy ${Gperftools_LIBRARIES}
)
