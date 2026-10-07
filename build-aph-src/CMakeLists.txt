cmake_minimum_required(VERSION 3.22)
project(EmberAphSmoke VERSION 0.1.0)
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
add_subdirectory("${JUCE_DIR}/extras/AudioPluginHost" "${CMAKE_BINARY_DIR}/aph")

juce_add_console_app(Vst3Smoke PRODUCT_NAME "Vst3Smoke")
target_sources(Vst3Smoke PRIVATE vst3_smoke.cpp)
target_compile_definitions(Vst3Smoke PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(Vst3Smoke PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics)
