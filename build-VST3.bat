:: Require Compiler: Visual Studio Build Tools 2026
:: Tick "Desktop development with C++" during install:
:: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022

:: ============================================================
:: PolyHost VST3 Plugin Build Script - Fully Portable
::
:: Layout expected inside the project folder:
::
::   tools\cmake\_4.4.2\bin\cmake.exe   <- portable CMake (no installer)
::   tools\vstsdk2.4\            <- (optional) VST2 SDK
::     pluginterfaces\vst2.x\aeffect.h
::
:: If tools\vstsdk2.4 is present, VST2 support is compiled in
:: automatically -- no editing of CMakeLists.txt required.
::
:: Requires:
::   ..\_Tools\cmake\_4.4.2\bin\cmake.exe
::   Visual Studio 2026 Build Tools
::   Desktop development with C++
::
:: The finished FX and Synth VST3 binaries are copied directly into dist
:: beside PolyHostInterface.exe. No other dist files or folders are removed.
:: ============================================================

@echo off
setlocal EnableExtensions

if /i "%~1"=="--run-build" goto :run_build

set "RESULTS_LOG=%~dp0Results.log"
set "PHI_BUILD_SCRIPT=%~f0"
set "PHI_BUILD_CAPTURE=1"

echo Starting PHI VST3 build...
echo Progress and warnings will appear below and be saved to Results.log.
echo.

where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo BUILD FAILED
    echo   - Windows PowerShell was not found
    echo.
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$writer = [System.IO.StreamWriter]::new($env:RESULTS_LOG, $false, [System.Text.UTF8Encoding]::new($false));" ^
  "$exitCode = 1;" ^
  "try {" ^
  "  & $env:PHI_BUILD_SCRIPT --run-build 2>&1 | ForEach-Object {" ^
  "    $line = $_.ToString();" ^
  "    [Console]::WriteLine($line);" ^
  "    $writer.WriteLine($line);" ^
  "    $writer.Flush();" ^
  "  };" ^
  "  $exitCode = $LASTEXITCODE;" ^
  "} finally { $writer.Dispose(); };" ^
  "exit $exitCode"
set "BUILD_EXIT_CODE=%ERRORLEVEL%"

echo.
echo Results saved:
echo   %RESULTS_LOG%
echo.
pause
exit /b %BUILD_EXIT_CODE%

:run_build
set "CMAKE_GENERATOR="
set "CMAKE_GENERATOR_PLATFORM="
set "CMAKE_GENERATOR_TOOLSET="

set "ROOT=%~dp0"
set "VERSION_FILE=%ROOT%Source\VST3\CMakeLists.txt"
set "POLYHOST_VERSION="
for /f "tokens=2" %%V in ('findstr /C:"set(POLYHOST_VERSION " "%VERSION_FILE%"') do set "POLYHOST_VERSION=%%V"
set "POLYHOST_VERSION=%POLYHOST_VERSION:"=%"
set "POLYHOST_VERSION=%POLYHOST_VERSION:)=%"
if not defined POLYHOST_VERSION (
    echo ERROR: Could not read POLYHOST_VERSION from:
    echo %VERSION_FILE%
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)
set "TOOLS=%ROOT%..\_Tools"
set "CMAKE=%TOOLS%\cmake\_4.4.2\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build-VST3"
set "DIST_DIR=%ROOT%dist"
set "FX_PLUGIN_NAME=PolyHostInterface.vst3"
set "FX_BUILT_BUNDLE=%BUILD_DIR%\PolyHostPlugin_artefacts\Release\VST3\%FX_PLUGIN_NAME%"
set "FX_BUILT_BINARY=%FX_BUILT_BUNDLE%\Contents\x86_64-win\%FX_PLUGIN_NAME%"
set "FX_FINAL_PLUGIN=%DIST_DIR%\%FX_PLUGIN_NAME%"
set "SYNTH_PLUGIN_NAME=PolyHostInterface_Synth.vst3"
set "SYNTH_BUILT_BUNDLE=%BUILD_DIR%\PolyHostSynthPlugin_artefacts\Release\VST3\%SYNTH_PLUGIN_NAME%"
set "SYNTH_BUILT_BINARY=%SYNTH_BUILT_BUNDLE%\Contents\x86_64-win\%SYNTH_PLUGIN_NAME%"
set "SYNTH_FINAL_PLUGIN=%DIST_DIR%\%SYNTH_PLUGIN_NAME%"
set "APP_NAME=savihost.exe"
set "SAVIHOST=D:\SyMenu\ProgramFiles\MyApps\MuLab\- VST's\Utilities\savihost\savihost.exe"
set "MIDI_ROUTING_TEST=%BUILD_DIR%\Release\PhiHostedMidiRoutingTests.exe"

echo ============================================================
echo PolyHost - VST3 Plugin Build Script - Building v%POLYHOST_VERSION%
echo ============================================================
echo.

if not exist "%CMAKE%" (
    echo ERROR: cmake.exe not found at:
    echo %CMAKE%
    echo.
    echo Expected shared tools folder:
    echo %TOOLS%
    echo.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio Build Tools not found.
    echo Install Visual Studio 2022 Community or Build Tools.
    echo Make sure Desktop development with C++ is installed.
    echo.
    if not defined PHI_BUILD_CAPTURE pause
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
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo.
echo Building and running hosted MIDI routing tests...
"%CMAKE%" --build "%BUILD_DIR%" --config Release --target PhiHostedMidiRoutingTests
if errorlevel 1 (
    echo.
    echo Hosted MIDI routing test build FAILED.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

if not exist "%MIDI_ROUTING_TEST%" (
    echo ERROR: Hosted MIDI routing test was not found at:
    echo %MIDI_ROUTING_TEST%
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

"%MIDI_ROUTING_TEST%"
if errorlevel 1 (
    echo.
    echo Hosted MIDI routing tests FAILED.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo.
echo Building Release...
"%CMAKE%" --build "%BUILD_DIR%" --config Release --target PolyHostPlugin_VST3 PolyHostSynthPlugin_VST3
if errorlevel 1 (
    echo.
    echo Build FAILED.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo.
echo Copying FX and Synth VST3 binaries directly to dist...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

if not exist "%FX_BUILT_BINARY%" (
    echo ERROR: Built FX VST3 binary not found at:
    echo %FX_BUILT_BINARY%
    echo.
    echo The build may have succeeded, but JUCE may have used a different artefacts path.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

if not exist "%SYNTH_BUILT_BINARY%" (
    echo ERROR: Built Synth VST3 binary not found at:
    echo %SYNTH_BUILT_BINARY%
    echo.
    echo The build may have succeeded, but JUCE may have used a different artefacts path.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

:: Remove only older PHI FX and Synth VST3 targets.
:: This also cleans up previous directory-bundle copies if present.
if exist "%FX_FINAL_PLUGIN%\NUL" rmdir /s /q "%FX_FINAL_PLUGIN%"
if exist "%FX_FINAL_PLUGIN%" del /f /q "%FX_FINAL_PLUGIN%"
if exist "%SYNTH_FINAL_PLUGIN%\NUL" rmdir /s /q "%SYNTH_FINAL_PLUGIN%"
if exist "%SYNTH_FINAL_PLUGIN%" del /f /q "%SYNTH_FINAL_PLUGIN%"

copy /Y "%FX_BUILT_BINARY%" "%FX_FINAL_PLUGIN%" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy FX VST3 binary to:
    echo %FX_FINAL_PLUGIN%
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

copy /Y "%SYNTH_BUILT_BINARY%" "%SYNTH_FINAL_PLUGIN%" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy Synth VST3 binary to:
    echo %SYNTH_FINAL_PLUGIN%
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

copy /Y "%ROOT%Source\FluentSystemIcons-LICENSE.txt" "%DIST_DIR%\FluentSystemIcons-LICENSE.txt" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy the Fluent System Icons licence to dist.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

if not exist "%FX_FINAL_PLUGIN%" (
    echo ERROR: Final FX VST3 binary was not copied successfully:
    echo %FX_FINAL_PLUGIN%
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

if not exist "%SYNTH_FINAL_PLUGIN%" (
    echo ERROR: Final Synth VST3 binary was not copied successfully:
    echo %SYNTH_FINAL_PLUGIN%
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo Copied VST3 editions to:
echo %FX_FINAL_PLUGIN%
echo %SYNTH_FINAL_PLUGIN%

:: Close cmd and launch this VST3 directly in Savihost. Do not use the
:: system .vst3 association, which belongs to PHI standalone.
echo.
echo ======================================================================
echo VST3 Build complete. Launching %SYNTH_PLUGIN_NAME% in 2 seconds...
echo ======================================================================
timeout /t 2 /nobreak >nul
if not exist "%SAVIHOST%" (
    echo WARNING: Savihost was not found at:
    echo %SAVIHOST%
    echo.
    echo VST3 build completed successfully, but PHI VST3 was not launched.
    exit /b 0
)
start "" "%SAVIHOST%" "%SYNTH_FINAL_PLUGIN%"
exit /b 0
