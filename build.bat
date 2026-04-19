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

set "ROOT=%~dp0"
set "CMAKE=%ROOT%tools\cmake\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build"
set "DIST_DIR=%ROOT%dist"
set "EXENAME=PolyHost.exe"
set "VST2_SDK=%ROOT%tools\vstsdk2.4"

:: -- Sanity check: CMake -----------------------------------------------
if not exist "%CMAKE%" (
    echo.
    echo  ERROR: cmake.exe not found at:
    echo         %CMAKE%
    echo.
    echo  Download the Windows x64 ZIP ^(no installer^) from:
    echo    https://cmake.org/download/
    echo  Extract so that tools\cmake\bin\cmake.exe exists.
    echo.
    pause
    exit /b 1
)

:: -- Sanity check: VS Build Tools --------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo.
    echo  ERROR: Visual Studio Build Tools not found.
    echo  Install VS Build Tools 2022 (free, compiler only) from:
    echo    https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
    echo  During install tick: Desktop development with C++
    echo.
    pause
    exit /b 1
)

:: -- VST2 SDK status ---------------------------------------------------
if exist "%VST2_SDK%\pluginterfaces\vst2.x\aeffect.h" (
    echo  VST2 SDK found -- VST2 support will be compiled in.
) else (
    echo  VST2 SDK not found at tools\vstsdk2.4\ -- building without VST2.
    echo  To add VST2: place the SDK so this file exists:
    echo    tools\vstsdk2.4\pluginterfaces\vst2.x\aeffect.h
)
echo.

cd /d "%ROOT%"

:: -- Close any running instance ----------------------------------------
echo Closing any running instances of %EXENAME%...
taskkill /IM "%EXENAME%" /F 2>nul

:: -- Clean previous build ----------------------------------------------
echo Cleaning previous build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

:: -- Configure ---------------------------------------------------------
echo.
echo Configuring (first run downloads JUCE -- needs internet)...

"%CMAKE%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo  Configure step FAILED. See errors above.
    pause
    exit /b 1
)

:: -- Build -------------------------------------------------------------
echo.
echo Building Release...

"%CMAKE%" --build "%BUILD_DIR%" --config Release
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo  Build FAILED. See errors above.
    pause
    exit /b 1
)

:: -- Copy output to dist\ ----------------------------------------------
echo.
echo Build successful! Copying to dist\...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
copy /Y "%BUILD_DIR%\PolyHost_artefacts\Release\%EXENAME%" "%DIST_DIR%\%EXENAME%"

echo.
echo Starting %EXENAME%...
timeout /t 2 /nobreak >nul
start "" "%DIST_DIR%\%EXENAME%"
exit
