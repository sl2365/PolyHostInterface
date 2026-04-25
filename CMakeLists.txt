cmake_minimum_required(VERSION 3.22)
project(PolyHost VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── VST2 SDK path (portable — sits inside the project tree) ──────────────────
# Place the SDK so this file exists:
#   tools\vstsdk2.4\pluginterfaces\vst2.x\aeffect.h
set(VST2_SDK_PATH "${CMAKE_SOURCE_DIR}/tools/vstsdk2.4")

add_subdirectory(tools/JUCE)
# add_subdirectory(tools/clap-juce-extensions)

# ── Application target ────────────────────────────────────────────────────────
juce_add_gui_app(PolyHost
    PRODUCT_NAME        "PolyHost"
    VERSION             "0.1.0"
    COMPANY_NAME        "PolyHost Project"
    BUNDLE_ID           "com.polyhost.app"
)

juce_generate_juce_header(PolyHost)

target_sources(PolyHost PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/PluginTabComponent.cpp
    Source/AudioEngine.cpp
    Source/MidiEngine.cpp
    Source/AppSettings.cpp
    Source/RoutingView.cpp
    Source/SessionManager.cpp
    Source/TransportController.cpp
)

# ── Detect whether the portable VST2 SDK is present ──────────────────────────
if(EXISTS "${VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h")
    message(STATUS "VST2 SDK found at ${VST2_SDK_PATH} -- VST2 support ENABLED")
    set(VST2_ENABLED 1)
else()
    message(STATUS "VST2 SDK not found at ${VST2_SDK_PATH} -- VST2 support DISABLED")
    message(STATUS "To enable VST2: place the SDK so tools/vstsdk2.4/pluginterfaces/vst2.x/aeffect.h exists")
    set(VST2_ENABLED 0)
endif()

target_compile_definitions(PolyHost PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1

    JUCE_PLUGINHOST_VST3=1          # VST3 x64 -- always enabled
#     JUCE_PLUGINHOST_CLAP=1          # CLAP -- via clap-juce-extensions

    # VST2 -- enabled automatically if tools\vstsdk2.4 is present
    $<$<BOOL:${VST2_ENABLED}>:JUCE_PLUGINHOST_VST=1>
    $<$<BOOL:${VST2_ENABLED}>:VST2_SDK_ROOT="${VST2_SDK_PATH}">
)

if(VST2_ENABLED)
	target_include_directories(PolyHost PRIVATE "${VST2_SDK_PATH}")
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
#         clap_juce_extensions
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)