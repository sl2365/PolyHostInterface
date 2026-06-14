:: Require Compiler: Visual Studio Build Tools 2026
:: Tick "Desktop development with C++" during install:
:: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022

:: ============================================================
:: PolyHost VST3 Plugin Build Script - Fully Portable
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

:: Requires:
::   - Desktop development with C++
:: PolyHost VST3 Plugin Build Script - Portable
:: Requires:
::   ..\_Tools\cmake\bin\cmake.exe
::   Visual Studio 2026 Build Tools
::   Desktop development with C++

@echo off
setlocal

set "ROOT=%~dp0"
set "TOOLS=%ROOT%..\_Tools"
set "CMAKE=%TOOLS%\cmake\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build-VST3"
set "FINAL_VST3_DIR=%ROOT%dist"
set "PLUGIN_NAME=PolyHostInterface.vst3"
set "PLUGIN_BINARY=%FINAL_VST3_DIR%\%PLUGIN_NAME%\Contents\x86_64-win\%PLUGIN_NAME%"
set "APP_NAME=savihost3x64.exe"

echo.
echo ============================================================
echo PolyHost VST3 Plugin Build Script
echo ============================================================
echo.

if not exist "%CMAKE%" (
    echo ERROR: cmake.exe not found at:
    echo %CMAKE%
    echo.
    echo Expected shared tools folder:
    echo %TOOLS%
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

cd /d "%ROOT%"

echo Closing any running instances of %APP_NAME%...
taskkill /IM "%APP_NAME%" /F 2>nul

echo Cleaning previous build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo.
echo Configuring plugin project...
"%CMAKE%" -S "%ROOT%Source\VST3" -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64
if errorlevel 1 (
    echo.
    echo Configure step FAILED.
    pause
    exit /b 1
)

echo.
echo Building Release...
"%CMAKE%" --build "%BUILD_DIR%" --config Release --target PolyHostPlugin_VST3
if errorlevel 1 (
    echo.
    echo Build FAILED.
    pause
    exit /b 1
)

echo.
echo Preparing final VST3 output folder...
if not exist "%FINAL_VST3_DIR%" mkdir "%FINAL_VST3_DIR%"

echo.
echo Copying VST3 plugin to simplified output path...
if exist "%BUILD_DIR%\PolyHostPlugin_artefacts\Release\VST3\%PLUGIN_NAME%" (
    if exist "%FINAL_VST3_DIR%\%PLUGIN_NAME%" rmdir /s /q "%FINAL_VST3_DIR%\%PLUGIN_NAME%"
    xcopy /E /I /Y "%BUILD_DIR%\PolyHostPlugin_artefacts\Release\VST3\%PLUGIN_NAME%" "%FINAL_VST3_DIR%\%PLUGIN_NAME%" >nul
    echo Copied VST3 to:
    echo %FINAL_VST3_DIR%\%PLUGIN_NAME%
) else (
    echo ERROR: VST3 plugin not found at expected path:
    echo %BUILD_DIR%\PolyHostPlugin_artefacts\Release\VST3\%PLUGIN_NAME%
    echo.
    echo The build may have succeeded, but JUCE may have used a different artefacts path.
    echo Please check inside the build folder for the generated .vst3 bundle.
    pause
    exit /b 1
)

if not exist "%PLUGIN_BINARY%" (
    echo ERROR: Final VST3 binary was not copied successfully:
    echo %PLUGIN_BINARY%
    pause
    exit /b 1
)

:: Close cmd and Launch plugin
echo.
echo =================================================================
echo VST3 Build complete. Launching %PLUGIN_NAME% in 2 seconds...
echo =================================================================
rem pause
timeout /t 2 /nobreak >nul
start "" "%PLUGIN_BINARY%"
exit /b 0
