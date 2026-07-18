cmake_minimum_required(VERSION 3.22)

# The standalone app compiles the current VST3 source directly.
# There are no duplicated PHI core/UI source files in Source/APP.
set(PHI_SHARED_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../VST3")

# Keep the standalone version number matched to Source/VST3/CMakeLists.txt.
file(STRINGS
    "${PHI_SHARED_SOURCE_DIR}/CMakeLists.txt"
    POLYHOST_VERSION_LINE
    REGEX "^[ \t]*set\\(POLYHOST_VERSION[ \t]+\"[0-9]+\\.[0-9]+\\.[0-9]+\"\\)"
    LIMIT_COUNT 1
)

string(REGEX MATCH
    "[0-9]+\\.[0-9]+\\.[0-9]+"
    POLYHOST_VERSION
    "${POLYHOST_VERSION_LINE}"
)

if(POLYHOST_VERSION STREQUAL "")
    message(FATAL_ERROR
        "Could not read POLYHOST_VERSION from ${PHI_SHARED_SOURCE_DIR}/CMakeLists.txt")
endif()

project(PolyHostInterfaceStandalone VERSION ${POLYHOST_VERSION} LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SHARED_TOOLS_DIR "${CMAKE_SOURCE_DIR}/../../../_Tools")
set(JUCE_SHARED_PATH "${SHARED_TOOLS_DIR}/JUCE")
set(VST2_SDK_PATH "${SHARED_TOOLS_DIR}/vstsdk2.4")

if(NOT EXISTS "${JUCE_SHARED_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE was not found at ${JUCE_SHARED_PATH}")
endif()

add_subdirectory("${JUCE_SHARED_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

# Detect the optional VST2 SDK for hosted VST2 plug-ins.
if(EXISTS "${VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h")
    message(STATUS "VST2 SDK found at ${VST2_SDK_PATH} -- VST2 hosting ENABLED")
    set(VST2_ENABLED 1)
else()
    message(STATUS "VST2 SDK not found at ${VST2_SDK_PATH} -- VST2 hosting DISABLED")
    set(VST2_ENABLED 0)
endif()

# This creates JUCE's standalone wrapper around the existing
# PolyHostPluginProcessor/createPluginFilter() implementation.
juce_add_plugin(PolyHost
    COMPANY_NAME "PolyHost Project"
    BUNDLE_ID "com.PolyHostInterface.app"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE PlHd
    PLUGIN_CODE Phpi
    FORMATS Standalone
    PRODUCT_NAME "PolyHostInterface"
)

juce_generate_juce_header(PolyHost)

if(WIN32)
    set(POLYHOST_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/PolyHostIcon.rc")

    file(WRITE "${POLYHOST_ICON_RC}"
        "1 ICON \"${CMAKE_CURRENT_LIST_DIR}/../app.ico\"\n"
    )

    target_sources(PolyHost_Standalone PRIVATE
        "${POLYHOST_ICON_RC}"
    )
endif()

target_sources(PolyHost PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/StandaloneApplication.cpp"
    "${PHI_SHARED_SOURCE_DIR}/AppSettings.cpp"
    "${PHI_SHARED_SOURCE_DIR}/AppSettings.h"
    "${PHI_SHARED_SOURCE_DIR}/PluginProcessor.cpp"
    "${PHI_SHARED_SOURCE_DIR}/PluginProcessor.h"
    "${PHI_SHARED_SOURCE_DIR}/PluginEditor.cpp"
    "${PHI_SHARED_SOURCE_DIR}/PluginEditor.h"
    "${PHI_SHARED_SOURCE_DIR}/MainView.cpp"
    "${PHI_SHARED_SOURCE_DIR}/MainView.h"
    "${PHI_SHARED_SOURCE_DIR}/Instructions.cpp"
    "${PHI_SHARED_SOURCE_DIR}/Instructions.h"
    "${PHI_SHARED_SOURCE_DIR}/PluginCore.cpp"
    "${PHI_SHARED_SOURCE_DIR}/PluginCore.h"
    "${PHI_SHARED_SOURCE_DIR}/TabModel.cpp"
    "${PHI_SHARED_SOURCE_DIR}/TabModel.h"
    "${PHI_SHARED_SOURCE_DIR}/SlotModel.cpp"
    "${PHI_SHARED_SOURCE_DIR}/SlotModel.h"
    "${PHI_SHARED_SOURCE_DIR}/SessionManager.cpp"
    "${PHI_SHARED_SOURCE_DIR}/SessionManager.h"
    "${PHI_SHARED_SOURCE_DIR}/ButtonStyling.cpp"
    "${PHI_SHARED_SOURCE_DIR}/ButtonStyling.h"
    "${PHI_SHARED_SOURCE_DIR}/PointerControl.cpp"
    "${PHI_SHARED_SOURCE_DIR}/PointerControl.h"
    "${PHI_SHARED_SOURCE_DIR}/PointerSettingsDialogue.cpp"
    "${PHI_SHARED_SOURCE_DIR}/PointerSettingsDialogue.h"
    "${PHI_SHARED_SOURCE_DIR}/RoutingView.cpp"
    "${PHI_SHARED_SOURCE_DIR}/RoutingView.h"
    "${PHI_SHARED_SOURCE_DIR}/MacroMappingsView.cpp"
    "${PHI_SHARED_SOURCE_DIR}/MacroMappingsView.h"
    "${PHI_SHARED_SOURCE_DIR}/DebugLog.cpp"
    "${PHI_SHARED_SOURCE_DIR}/DebugLog.h"
)

target_include_directories(PolyHost PRIVATE
    "${PHI_SHARED_SOURCE_DIR}"
)

target_compile_definitions(PolyHost
    PUBLIC
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1

    PRIVATE
        POLYHOST_VERSION_STRING="${POLYHOST_VERSION}"
        POLYHOST_STANDALONE_BUILD=1
        POLYHOST_VST3_BUILD=0

        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_VST3_CAN_REPLACE_VST2=0

        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LV2=0

        $<$<BOOL:${VST2_ENABLED}>:JUCE_PLUGINHOST_VST=1>
        $<$<BOOL:${VST2_ENABLED}>:VST2_SDK_ROOT="${VST2_SDK_PATH}">
)

if(VST2_ENABLED)
    target_include_directories(PolyHost PRIVATE "${VST2_SDK_PATH}")
endif()

if(MSVC)
    target_link_options(PolyHost_Standalone PRIVATE "/DYNAMICBASE:NO")
endif()

target_link_libraries(PolyHost
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
