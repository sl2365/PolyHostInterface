cmake_minimum_required(VERSION 3.22)

# The isolated Synth32 VST3 compiles the existing PHI source directly.
# The established 64-bit FX/Synth and standalone configurations are untouched.
set(PHI_SHARED_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../VST3")

# Keep every PHI edition on the version declared by the main VST3 project.
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

project(PolyHostInterfaceSynth32 VERSION ${POLYHOST_VERSION} LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "PolyHostInterface_Synth32 requires a 32-bit generator such as -A Win32")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SHARED_TOOLS_DIR "${CMAKE_SOURCE_DIR}/../../../_Tools")
set(JUCE_SHARED_PATH "${SHARED_TOOLS_DIR}/JUCE/_9.0.4")
set(VST2_SDK_PATH "${SHARED_TOOLS_DIR}/vstsdk2.4")

if(NOT EXISTS "${JUCE_SHARED_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE was not found at ${JUCE_SHARED_PATH}")
endif()

add_subdirectory("${JUCE_SHARED_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

# Synth32 exists to host legacy 32-bit plug-ins, so VST2.4 hosting is required.
if(NOT EXISTS "${VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h")
    message(FATAL_ERROR
        "Synth32 requires the VST2 SDK at ${VST2_SDK_PATH}")
endif()

message(STATUS
    "VST2 SDK found at ${VST2_SDK_PATH} -- 32-bit VST2 hosting ENABLED")

juce_add_plugin(PolyHostSynth32
    COMPANY_NAME "sl23"
    BUNDLE_ID "com.sl23.polyhostinterfacesynth32"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Sl23
    PLUGIN_CODE P32s
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
    PRODUCT_NAME "PolyHostInterface_Synth32"
)

juce_generate_juce_header(PolyHostSynth32)

set(POLYHOST_SYNTH32_SOURCES
    "${PHI_SHARED_SOURCE_DIR}/AppSettings.cpp"
    "${PHI_SHARED_SOURCE_DIR}/AppSettings.h"
    "${PHI_SHARED_SOURCE_DIR}/PluginProcessor.cpp"
    "${PHI_SHARED_SOURCE_DIR}/PluginProcessor.h"
    "${PHI_SHARED_SOURCE_DIR}/PluginEditor.cpp"
    "${PHI_SHARED_SOURCE_DIR}/PluginEditor.h"
    "${PHI_SHARED_SOURCE_DIR}/MainView.cpp"
    "${PHI_SHARED_SOURCE_DIR}/MainView.h"
    "${PHI_SHARED_SOURCE_DIR}/HostedMidiRouting.h"
    "${PHI_SHARED_SOURCE_DIR}/MidiKeyboardPanel.cpp"
    "${PHI_SHARED_SOURCE_DIR}/MidiKeyboardPanel.h"
    "${PHI_SHARED_SOURCE_DIR}/Instructions.cpp"
    "${PHI_SHARED_SOURCE_DIR}/Instructions.h"
    "${PHI_SHARED_SOURCE_DIR}/PluginCore.cpp"
    "${PHI_SHARED_SOURCE_DIR}/PluginCore.h"
    "${PHI_SHARED_SOURCE_DIR}/SeqwencerBridgeProtocol.h"
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
    "${PHI_SHARED_SOURCE_DIR}/AdvancedRoutingView.cpp"
    "${PHI_SHARED_SOURCE_DIR}/AdvancedRoutingView.h"
    "${PHI_SHARED_SOURCE_DIR}/MacroMappingsView.cpp"
    "${PHI_SHARED_SOURCE_DIR}/MacroMappingsView.h"
    "${PHI_SHARED_SOURCE_DIR}/AudioRecordingController.cpp"
    "${PHI_SHARED_SOURCE_DIR}/AudioRecordingController.h"
    "${PHI_SHARED_SOURCE_DIR}/MidiRecordingController.cpp"
    "${PHI_SHARED_SOURCE_DIR}/MidiRecordingController.h"
    "${PHI_SHARED_SOURCE_DIR}/RecordingView.cpp"
    "${PHI_SHARED_SOURCE_DIR}/RecordingView.h"
    "${PHI_SHARED_SOURCE_DIR}/DebugLog.cpp"
    "${PHI_SHARED_SOURCE_DIR}/DebugLog.h"
)

target_sources(PolyHostSynth32 PRIVATE
    ${POLYHOST_SYNTH32_SOURCES}
)

target_include_directories(PolyHostSynth32 PRIVATE
    "${PHI_SHARED_SOURCE_DIR}"
    "${VST2_SDK_PATH}"
)

target_compile_definitions(PolyHostSynth32 PRIVATE
    POLYHOST_VERSION_STRING="${POLYHOST_VERSION}"
    POLYHOST_STANDALONE_BUILD=0
    POLYHOST_VST3_BUILD=1
    POLYHOST_32BIT_EDITION=1

    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_VST3_CAN_REPLACE_VST2=0

    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=1
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LV2=0
    VST2_SDK_ROOT="${VST2_SDK_PATH}"

    JucePlugin_Build_VST=0
    JucePlugin_Build_VST3=1
    JucePlugin_Build_AU=0
    JucePlugin_Build_AUv3=0
    JucePlugin_Build_AAX=0
    JucePlugin_Build_Standalone=0
    JucePlugin_Build_Unity=0
    JucePlugin_Build_LV2=0
)

target_link_libraries(PolyHostSynth32
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
