# Poly Host Interface

A lightweight, tabbed VST3 / CLAP plugin host for Windows.
Play synths and route FX chains, driven by any MIDI device.
Fully portable: stores all settings next to the exe, touches nothing else on the system.

I always liked tools like TobyBear's MiniHost, SaviHost and Tone2's NanoHost. But wanted something that combines all of them. A quick jamming tool that can open multiple plugin formats. I aim to add support for more formats, but the main ones will most likely be tried first at least. This is a tool that doesn't yet exist elsewhere in a small package like this, at least not as one that you can use file-associations with.

## Feature Status

| Feature | Status |
|---|---|
| VST3 x64 | ✅ Working |
| CLAP x64 | ✅ Working (via clap-juce-extensions) |
| VST2 x64 | ✅ Working - Requires Steinberg VST2 SDK — see below |
| VST2/VST3 **32-bit** in 64-bit host | 🔴 Needs plugin bridge — see below - planned |
| MIDI 1.0 | ✅ Working |
| MIDI 2.0 (Windows MIDI Services) | ⚠️ Requires JUCE 8+ and Windows 11 - planned |
| Tabbed interface (Synth = parallel + FX = serial) | ✅ Working |
| Tab-order = FX routing order | ✅ Working |
| Portable settings (no AppData/registry) | ✅ Working |
| Audio/MIDI recording | 🔲 Planned |

## Folder Structure

```
PolyHost\                        ← your project root
│
├── build.bat                    ← THE build script (run this)
├── CMakeLists.txt
├── README.md
│
├── Source\
│   ├── Main.cpp
│   ├── MainComponent.h / .cpp
│   ├── PluginTabComponent.h / .cpp
│   ├── AudioEngine.h / .cpp
│   ├── MidiEngine.h / .cpp
│   └── AppSettings.h / .cpp
│
├── tools\
│   ├── cmake\                   ← extract cmake-3.x.x-windows-x86_64.zip HERE
│   │   └── bin\
│   │       └── cmake.exe
│   │
│   └── vstsdk2.4\      ← drop your SDK here
│       └── pluginterfaces\
│           └── vst2.x\
│               └── aeffect.h   ← CMake checks for this file
│
├── dist\                        ← build output — PolyHost.exe lands here
└── [PolyHost.exe]\
    └── Settings\                ← created at runtime, settings stored here
        └── polyhost.xml
```

## Build Setup (one-time)

### 1. Visual Studio 2022 Community (free)
https://visualstudio.microsoft.com/vs/community/
During install tick: Desktop development with C++
You never open Visual Studio. It only provides the C++ compiler that CMake calls.
But this way allows you to use whatever editor you want.

### 2. Portable CMake (no installer)
Download the Windows x64 ZIP from: https://cmake.org/download/
Extract so the result is: tools\cmake\bin\cmake.exe

### 3. Build
Double-click build.bat from the project root.
First run downloads JUCE automatically (needs internet).
After that, builds are fully offline.
The finished exe appears in dist\PolyHost.exe

## Opening a Plugin

1. Right-click any .vst3 or .clap file in File Explorer
2. Open with > Choose another app
3. Browse to dist\PolyHost.exe
4. Tick "Always use this app" if you want it permanent

## VST2 Support

If you have a copy of vstsdk2.4, place it e.g. at C:\SDKs\vstsdk2.4
then in CMakeLists.txt uncomment:
    JUCE_PLUGINHOST_VST=1
    VST2_SDK_ROOT="C:/SDKs/vstsdk2.4"
and run build.bat again.

## Audio Routing

    MIDI Device
         |
    [Synth Tab 1] --+
    [Synth Tab 2] --+  (summed)
                    |
              [FX Tab 1]
                    |
              [FX Tab 2]  ...
                    |
              [Audio Out]

Drag FX tabs left/right on the tab bar to change processing order.
