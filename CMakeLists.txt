cmake_minimum_required(VERSION 3.22)

project(THEGRID VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if(THEGRID_JUCE_DIR)
    add_subdirectory(${THEGRID_JUCE_DIR} juce)
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

option(THEGRID_BUILD_TESTS "Build the offline engine test, MIDI render and UI snapshot" OFF)

set(THEGRID_FORMATS AU VST3)
if(NOT APPLE)
    set(THEGRID_FORMATS VST3)
endif()

# A MIDI effect: in Logic it goes in a track's MIDI FX slot, in front of the instrument it plays.
juce_add_plugin(THEGRID
    COMPANY_NAME                "manners! DSP"
    BUNDLE_ID                   com.mannersdsp.thegrid
    PLUGIN_MANUFACTURER_CODE    Mnrs
    PLUGIN_CODE                 Tgrd
    PRODUCT_NAME                "THE GRID"
    DESCRIPTION                 "Step sequencer"
    FORMATS                     ${THEGRID_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    AU_MAIN_TYPE                kAudioUnitType_MIDIProcessor
    VST3_CATEGORIES             Fx)

juce_add_binary_data(THEGRID_Assets SOURCES Resources/ShareTechMono-Regular.ttf)

target_sources(THEGRID PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(THEGRID PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(THEGRID
    PRIVATE
        THEGRID_Assets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(THEGRID_BUILD_TESTS)
    juce_add_console_app(THEGRID_Test PRODUCT_NAME "THEGRID_Test")
    target_sources(THEGRID_Test PRIVATE Tests/OfflineTest.cpp)
    target_include_directories(THEGRID_Test PRIVATE Source)
    target_compile_definitions(THEGRID_Test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(THEGRID_Test PRIVATE juce::juce_core juce::juce_recommended_config_flags)

    juce_add_console_app(THEGRID_Tool PRODUCT_NAME "THEGRID_Tool")
    target_sources(THEGRID_Tool PRIVATE
        Tests/Tool.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(THEGRID_Tool PRIVATE Source)
    target_compile_definitions(THEGRID_Tool PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(THEGRID_Tool PRIVATE
        THEGRID_Assets
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_recommended_config_flags)
endif()
