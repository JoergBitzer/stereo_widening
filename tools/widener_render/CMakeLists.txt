# Headless console tool: renders a wav file through one of the real StereoWidener
# algorithm classes (not the Python reference) and writes the result to another wav
# file, so the actual plugin DSP can be measured with python/stereo_eval the same way
# tools/meter_crosscheck cross-checks the meters (see Phase 3, "Render the test signals
# through the plugin and run the evaluation report" in plan2.md).
# No GUI/plugin-host is needed, which makes it usable in CI and in this sandbox (the
# StereoWidener Standalone GUI currently cannot run here, see stereo_widening/README.md).

cmake_minimum_required(VERSION 3.15)
project(WidenerRender VERSION 0.1.0)

juce_add_console_app(WidenerRender
    PRODUCT_NAME "WidenerRender")

juce_generate_juce_header(WidenerRender)

target_sources(WidenerRender
    PRIVATE
        main.cpp
        ../../StereoWidener/algorithms/MSWidthBroadband.cpp
        ../../StereoWidener/algorithms/MSWidthFiltered.cpp
        ../../StereoWidener/algorithms/ComplementaryComb.cpp)

target_compile_definitions(WidenerRender
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_include_directories(WidenerRender PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../StereoWidener"
        "${CMAKE_CURRENT_SOURCE_DIR}/../..")

target_link_libraries(WidenerRender
    PRIVATE
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
