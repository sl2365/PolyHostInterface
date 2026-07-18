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
:: Requires:
::   ..\_Tools\cmake\bin\cmake.exe
::   Visual Studio 2026 Build Tools
::   Desktop development with C++
::
:: The finished VST3 binary is copied directly into dist beside
:: PolyHostInterface.exe. No other dist files or folders are removed.
:: ============================================================

@echo off
setlocal

set "ROOT=%~dp0"
set "TOOLS=%ROOT%..\_Tools"
set "CMAKE=%TOOLS%\cmake\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build-VST3"
set "DIST_DIR=%ROOT%dist"
set "PLUGIN_NAME=PolyHostInterface.vst3"
set "BUILT_BUNDLE=%BUILD_DIR%\PolyHostPlugin_artefacts\Release\VST3\%PLUGIN_NAME%"
set "BUILT_BINARY=%BUILT_BUNDLE%\Contents\x86_64-win\%PLUGIN_NAME%"
set "FINAL_PLUGIN=%DIST_DIR%\%PLUGIN_NAME%"
set "APP_NAME=savihost3x64.exe"

echo.
echo ============================================================
echo PolyHost - VST3 Plugin Build Script
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
echo Copying VST3 binary directly to dist...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

if not exist "%BUILT_BINARY%" (
    echo ERROR: Built VST3 binary not found at:
    echo %BUILT_BINARY%
    echo.
    echo The build may have succeeded, but JUCE may have used a different artefacts path.
    pause
    exit /b 1
)

:: Remove only an older PolyHostInterface.vst3 target.
:: This also cleans up the previous directory-bundle copy if present.
if exist "%FINAL_PLUGIN%\NUL" rmdir /s /q "%FINAL_PLUGIN%"
if exist "%FINAL_PLUGIN%" del /f /q "%FINAL_PLUGIN%"

copy /Y "%BUILT_BINARY%" "%FINAL_PLUGIN%" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy VST3 binary to:
    echo %FINAL_PLUGIN%
    pause
    exit /b 1
)

if not exist "%FINAL_PLUGIN%" (
    echo ERROR: Final VST3 binary was not copied successfully:
    echo %FINAL_PLUGIN%
    pause
    exit /b 1
)

echo Copied VST3 to:
echo %FINAL_PLUGIN%

:: Close cmd and launch plugin
echo.
echo ======================================================================
echo VST3 Build complete. Launching %PLUGIN_NAME% in 2 seconds...
echo ======================================================================
timeout /t 2 /nobreak >nul
start "" "%FINAL_PLUGIN%"
exit /b 0
