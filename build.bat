:: First run downloads JUCE into build folder - fully portable.
:: Require Compiler: Visual Studio Build Tools 2022
:: Tick "Desktop development with C++" during install:
:: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022

@echo off
:: ============================================================
:: PolyHost Build Script - Fully Portable
::
:: Layout expected inside the project folder:
::
::   tools\cmake\bin\cmake.exe   <- portable CMake (no installer)
::   tools\vstsdk2.4\            <- (optional) VST2 SDK
::     pluginterfaces\vst2.x\aeffect.h
::
:: If tools\vstsdk2.4 is present, VST2 support is compiled in
:: automatically -- no editing of CMakeLists.txt required.
::
:: JUCE and clap-juce-extensions are downloaded into build\ on
:: the first run (needs internet). All subsequent builds are
:: fully offline.
:: ============================================================

setlocal

set "ROOT=%~dp0"
set "CMAKE=%ROOT%tools\cmake\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build"
set "DIST_DIR=%ROOT%dist"
set "EXENAME=PolyHostInterface.exe"
set "VST2_SDK=%ROOT%tools\vstsdk2.4"

echo.
echo ============================================================
echo PolyHost Build Script
echo ============================================================
echo.

if not exist "%CMAKE%" (
    echo ERROR: cmake.exe not found at:
    echo %CMAKE%
    echo.
    echo Download the Windows x64 ZIP from:
    echo https://cmake.org/download/
    echo Extract it so this file exists:
    echo tools\cmake\bin\cmake.exe
    echo.
    pause
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio Build Tools not found.
    echo Install Visual Studio 2022 Community or Build Tools.
    echo Make sure Desktop development with C++ is installed.
    echo.
    pause
    exit /b 1
)

if exist "%VST2_SDK%\pluginterfaces\vst2.x\aeffect.h" (
    echo VST2 SDK found - VST2 support will be compiled in.
) else (
    echo VST2 SDK not found at tools\vstsdk2.4 - building without VST2.
    echo To add VST2, place this file here:
    echo tools\vstsdk2.4\pluginterfaces\vst2.x\aeffect.h
)
echo.

cd /d "%ROOT%"

echo Closing any running instances of %EXENAME%...
taskkill /IM "%EXENAME%" /F 2>nul

echo Cleaning previous build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo.
echo Configuring project...
"%CMAKE%" -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64
if errorlevel 1 (
    echo.
    echo Configure step FAILED.
    pause
    exit /b 1
)

echo.
echo Building Release...
"%CMAKE%" --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo.
    echo Build FAILED.
    pause
    exit /b 1
)

echo.
echo Copying output to dist...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

if exist "%BUILD_DIR%\PolyHost_artefacts\Release\%EXENAME%" (
    copy /Y "%BUILD_DIR%\PolyHost_artefacts\Release\%EXENAME%" "%DIST_DIR%\%EXENAME%"
) else (
    echo Expected EXE not found at:
    echo %BUILD_DIR%\PolyHost_artefacts\Release\%EXENAME%
    echo.
    echo Build may have succeeded with a different output path.
    pause
    exit /b 1
)

echo.
echo Build complete. Launching %EXENAME% in 2 seconds...
timeout /t 2 /nobreak >nul
start "" "%DIST_DIR%\%EXENAME%"
exit