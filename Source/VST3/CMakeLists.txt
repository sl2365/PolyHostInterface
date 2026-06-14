cmake_minimum_required(VERSION 3.22)

set(POLYHOST_VERSION "1.1.14")

project(PolyHostInterface VERSION ${POLYHOST_VERSION} LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SHARED_TOOLS_DIR "${CMAKE_SOURCE_DIR}/../../../_Tools")
set(JUCE_SHARED_PATH "${SHARED_TOOLS_DIR}/JUCE")

add_subdirectory("${JUCE_SHARED_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(PolyHostPlugin
    COMPANY_NAME "PolyHost Project"
    BUNDLE_ID "com.PolyHostInterface.plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE PlHd
    PLUGIN_CODE Phpi
    FORMATS VST3
    VST3_CATEGORIES Fx
    PRODUCT_NAME "PolyHostInterface"
)

juce_generate_juce_header(PolyHostPlugin)

target_sources(PolyHostPlugin PRIVATE
    AppSettings.cpp
    AppSettings.h
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    MainView.cpp
    MainView.h
    PluginCore.cpp
    PluginCore.h
    TabModel.cpp
    TabModel.h
    SlotModel.cpp
    SlotModel.h
    SessionManager.cpp
    SessionManager.h
    ButtonStyling.cpp
    ButtonStyling.h
    PointerControl.cpp
    PointerControl.h
    RoutingView.cpp
    RoutingView.h
    DebugLog.cpp
    DebugLog.h
)

target_compile_definitions(PolyHostPlugin PRIVATE
    POLYHOST_VERSION_STRING="${POLYHOST_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_VST3_CAN_REPLACE_VST2=0

    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LV2=0

    JucePlugin_Build_VST=0
    JucePlugin_Build_VST3=1
    JucePlugin_Build_AU=0
    JucePlugin_Build_AUv3=0
    JucePlugin_Build_AAX=0
    JucePlugin_Build_Standalone=0
    JucePlugin_Build_Unity=0
    JucePlugin_Build_LV2=0
)

target_link_libraries(PolyHostPlugin
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)