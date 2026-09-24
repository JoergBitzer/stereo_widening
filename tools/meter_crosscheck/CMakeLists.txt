# Headless console tool: feeds a wav file through StereoMeterState in host-sized blocks
# and prints the resulting meter values, so they can be cross-checked against
# python/stereo_eval (see Phase 2, "cross-check against Python" in plan2.md).
# No GUI/display is needed, which makes it usable in CI and in this sandbox (the
# StereoAnalyzer Standalone GUI currently cannot run here, see stereo_widening/README.md).

cmake_minimum_required(VERSION 3.15)
project(MeterCrossCheck VERSION 0.1.0)

juce_add_console_app(MeterCrossCheck
    PRODUCT_NAME "MeterCrossCheck")

juce_generate_juce_header(MeterCrossCheck)

target_sources(MeterCrossCheck
    PRIVATE
        main.cpp
        ../../shared/metering/StereoMeterState.cpp
        ../../shared/metering/MeterFifo.cpp)

target_compile_definitions(MeterCrossCheck
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_include_directories(MeterCrossCheck PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")

target_link_libraries(MeterCrossCheck
    PRIVATE
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
