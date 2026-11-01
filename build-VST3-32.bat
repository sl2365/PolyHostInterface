:: Build the isolated 32-bit Synth VST3 for bridged use in FL Studio.
:: This script does not use or modify the existing 64-bit build folders.
:: Requires Visual Studio 2026 with Desktop development with C++,
:: the MSVC x86 build tools and the VST2.4 SDK.

@echo off
setlocal EnableExtensions

if /i "%~1"=="--run-build" goto :run_build

set "RESULTS_LOG=%~dp0Results.log"
set "PHI_BUILD_SCRIPT=%~f0"
set "PHI_BUILD_CAPTURE=1"

echo Starting PHI Synth32 VST3 build...
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

for %%I in ("%ROOT%..") do set "PROJECTS_ROOT=%%~fI"
set "TOOLS=%PROJECTS_ROOT%\_Tools"
set "CMAKE=%TOOLS%\cmake\_4.4.2\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build-VST3-32"
set "DIST_DIR=%ROOT%dist"
set "PLUGIN_NAME=PolyHostInterface_Synth32.vst3"
set "BUILT_BUNDLE=%BUILD_DIR%\PolyHostSynth32_artefacts\Release\VST3\%PLUGIN_NAME%"
set "BUILT_BINARY=%BUILT_BUNDLE%\Contents\x86-win\%PLUGIN_NAME%"
set "FINAL_PLUGIN=%DIST_DIR%\%PLUGIN_NAME%"
set "VST2_SDK=%TOOLS%\vstsdk2.4"

echo ============================================================
echo PolyHost - 32-bit Synth VST3 - Building v%POLYHOST_VERSION%
echo ============================================================
echo.

if not exist "%CMAKE%" (
    echo ERROR: cmake.exe not found at:
    echo %CMAKE%
    echo.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio Build Tools not found.
    echo Install Visual Studio 2026 Community or Build Tools.
    echo Make sure Desktop development with C++ and the MSVC x86 tools are installed.
    echo.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

if not exist "%VST2_SDK%\pluginterfaces\vst2.x\aeffect.h" (
    echo ERROR: VST2 SDK not found at ..\_Tools\vstsdk2.4
    echo Synth32 requires this file to host 32-bit VST2.4 plugins:
    echo ..\_Tools\vstsdk2.4\pluginterfaces\vst2.x\aeffect.h
    echo.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

cd /d "%ROOT%"

echo Cleaning previous isolated Synth32 build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo.
echo Configuring isolated 32-bit Synth VST3 project...
"%CMAKE%" -S "%ROOT%Source\VST3-32" -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A Win32
if errorlevel 1 (
    echo.
    echo Configure step FAILED.
    echo Confirm the MSVC x86 build tools are installed in Visual Studio.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo.
echo Building 32-bit Synth VST3 Release...
"%CMAKE%" --build "%BUILD_DIR%" --config Release --target PolyHostSynth32_VST3
if errorlevel 1 (
    echo.
    echo Build FAILED.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo.
echo Copying Synth32 VST3 to dist...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

if not exist "%BUILT_BINARY%" (
    echo ERROR: Built Synth32 VST3 binary not found at:
    echo %BUILT_BINARY%
    echo.
    echo The build may have succeeded, but JUCE may have used a different artefacts path.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

if exist "%FINAL_PLUGIN%\NUL" rmdir /s /q "%FINAL_PLUGIN%"
if exist "%FINAL_PLUGIN%" del /f /q "%FINAL_PLUGIN%"

copy /Y "%BUILT_BINARY%" "%FINAL_PLUGIN%" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy Synth32 VST3 to:
    echo %FINAL_PLUGIN%
    echo Close FL Studio or any other host using the plug-in, then build again.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

copy /Y "%ROOT%Source\FluentSystemIcons-LICENSE.txt" "%DIST_DIR%\FluentSystemIcons-LICENSE.txt" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy the Fluent System Icons licence to dist.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo.
echo Copied Synth32 VST3 to:
echo %FINAL_PLUGIN%
echo.
echo ============================================================
echo PHI Synth32 VST3 build completed successfully.
echo ============================================================
exit /b 0
