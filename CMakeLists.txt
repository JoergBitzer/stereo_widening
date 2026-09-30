# Standalone build of this repository (used by the GitHub release workflow):
#
#   git clone --recursive https://github.com/JoergBitzer/stereo_widening.git
#   cmake -S stereo_widening -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release --target StereoWidener_VST3 StereoWidener_Standalone
#
# JUCE comes as a git submodule (JUCE/, pinned to the release used for the binaries).
# In the AudioDev environment (see README.md) this file is not used: AudioDev's own
# top-level CMakeLists.txt adds StereoWidener/ and the tools directly, with its JUCE.

cmake_minimum_required(VERSION 3.22)

project(StereoWidening VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found: run 'git submodule update --init' first")
endif()
add_subdirectory(JUCE)

add_subdirectory(StereoWidener)

# optional: the other plugin and the test tools of this repository
option(STEREO_WIDENING_BUILD_EXTRAS "Also build StereoAnalyzer, WidenerRender and MeterCrossCheck" OFF)
if(STEREO_WIDENING_BUILD_EXTRAS)
    add_subdirectory(StereoAnalyzer)
    add_subdirectory(tools/widener_render)
    add_subdirectory(tools/meter_crosscheck)
endif()
