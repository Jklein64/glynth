cmake_minimum_required(VERSION 3.22)
project(glynth VERSION 0.0.1 LANGUAGES C CXX)

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
FetchContent_Declare(
    efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG master
)
FetchContent_MakeAvailable(
    freetype
    fmt
    glm
    libnpy
    juce
    efsw
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Gperftools COMPONENTS PROFILER)

add_executable(read src/read.cpp src/outliner.cpp)
target_compile_features(read PRIVATE cxx_std_20)
target_link_libraries(
    read
    PRIVATE freetype fmt::fmt glm::glm npy::npy ${Gperftools_LIBRARIES}
)

juce_add_plugin(
    glynth
    COMPANY_NAME "Jason Klein"
    BUNDLE_ID "com.jklein.glynth"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Jk64
    PLUGIN_CODE Glyn
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    FORMATS AU Standalone
    PRODUCT_NAME glynth
)

file(
    GLOB SHADER_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/shader/*.glsl
)
juce_add_binary_data(
    shaders
    HEADER_NAME "shaders.h"
    NAMESPACE shaders
    SOURCES
    ${SHADER_FILES}
)

target_sources(
    glynth
    PRIVATE src/editor.cpp src/processor.cpp src/shader_manager.cpp
)
target_compile_features(glynth PRIVATE cxx_std_20)
option(GLYNTH_HOT_SHADER_RELOAD "Enable hot reloading of shaders" OFF)
message("GLYNTH_HOT_SHADER_RELOAD = ${GLYNTH_HOT_SHADER_RELOAD}")
# Generator expression for Hot Shader Reloading (HSR)
set(HSR_GEN $<BOOL:${GLYNTH_HOT_SHADER_RELOAD}>)
target_compile_definitions(
    glynth
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DONT_ASSERT_ON_GLSL_COMPILE_ERROR=1
        $<${HSR_GEN}:GLYNTH_HSR>
        $<${HSR_GEN}:GLYNTH_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shader">
)

target_link_libraries(
    glynth
    PRIVATE shaders
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

include(cmake/utils.cmake)
target_link_libraries_system(
    glynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_opengl
        fmt::fmt
        glm::glm
        efsw-static
)
