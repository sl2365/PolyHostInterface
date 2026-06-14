cmake_minimum_required(VERSION 3.22)

set(POLYHOST_VERSION "1.0.0")

project(PolyHostInterface VERSION ${POLYHOST_VERSION} LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── VST2 SDK path (portable — shared central tools folder) ───────────────────
# Shared tools folder layout expected:
#   ..\_Tools\vstsdk2.4\
#     pluginterfaces\vst2.x\aeffect.h
#   ..\_Tools\JUCE\
get_filename_component(PROJECTS_ROOT "${CMAKE_SOURCE_DIR}/../../.." ABSOLUTE)
set(SHARED_TOOLS_PATH "${PROJECTS_ROOT}/_Tools")
set(VST2_SDK_PATH "${SHARED_TOOLS_PATH}/vstsdk2.4")
set(JUCE_PATH "${SHARED_TOOLS_PATH}/JUCE")
set(CLAP_PATH "${SHARED_TOOLS_PATH}/clap")

add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

# ── Application target ────────────────────────────────────────────────────────
juce_add_gui_app(PolyHost
    PRODUCT_NAME        "PolyHostInterface"
    VERSION             "${POLYHOST_VERSION}"
    COMPANY_NAME        "PolyHost Project"
    BUNDLE_ID           "com.PolyHostInterface.app"
)

juce_generate_juce_header(PolyHost)

target_sources(PolyHost PRIVATE
    Main.cpp
    MainComponent.cpp
    DebugLog.cpp
    PluginTabComponent.cpp
    AudioEngine.cpp
    MidiEngine.cpp
    AppSettings.cpp
    RoutingView.cpp
    SessionManager.cpp
    StandaloneTempoSupport.cpp
    PolyHost.rc
    ClapPluginWrapper.cpp
    ClapPluginWrapper.h
    ButtonStyling.cpp
    ButtonStyling.h
    PointerControl.cpp
    PointerControl.h
)

# ── Detect whether the portable VST2 SDK is present ──────────────────────────
if(EXISTS "${VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h")
    message(STATUS "VST2 SDK found at ${VST2_SDK_PATH} -- VST2 support ENABLED")
    set(VST2_ENABLED 1)
else()
    message(STATUS "VST2 SDK not found at ${VST2_SDK_PATH} -- VST2 support DISABLED")
    message(STATUS "To enable VST2: place the SDK so ${VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h exists")
    set(VST2_ENABLED 0)
endif()

target_compile_definitions(PolyHost PRIVATE
    POLYHOST_VERSION_STRING="${POLYHOST_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1

    JUCE_PLUGINHOST_VST3=1          # VST3 x64 -- always enabled

    # VST2 -- enabled automatically if ..\_Tools\vstsdk2.4 is present
    $<$<BOOL:${VST2_ENABLED}>:JUCE_PLUGINHOST_VST=1>
    $<$<BOOL:${VST2_ENABLED}>:VST2_SDK_ROOT="${VST2_SDK_PATH}">
)

if(VST2_ENABLED)
    target_include_directories(PolyHost PRIVATE "${VST2_SDK_PATH}")
endif()

target_include_directories(PolyHost PRIVATE
    "${CLAP_PATH}/include"
)

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