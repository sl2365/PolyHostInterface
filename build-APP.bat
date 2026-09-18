:: Build script using shared central tools folder.
:: Require Compiler: Visual Studio Build Tools 2022
:: Tick "Desktop development with C++" during install:
:: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022

@echo off
:: ============================================================
:: PolyHost Build Script - Fully Portable
::
:: Layout expected:
::
::   ..\_Tools\cmake\_4.4.2\bin\cmake.exe   <- portable CMake (no installer)
::   ..\_Tools\vstsdk2.4\            <- (optional) VST2 SDK
::     pluginterfaces\vst2.x\aeffect.h
::
:: If ..\_Tools\vstsdk2.4 is present, VST2 support is compiled in
:: automatically -- no editing of CMakeLists.txt required.
::
:: JUCE is loaded from the shared tools folder by CMake.
:: ============================================================

setlocal EnableExtensions

if /i "%~1"=="--run-build" goto :run_build

set "RESULTS_LOG=%~dp0Results.log"
set "PHI_BUILD_SCRIPT=%~f0"
set "PHI_BUILD_CAPTURE=1"

echo Starting PHI standalone build...
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
for %%I in ("%ROOT%..") do set "PROJECTS_ROOT=%%~fI"
set "TOOLS=%PROJECTS_ROOT%\_Tools"
set "CMAKE=%TOOLS%\cmake\_4.4.2\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build-APP"
set "DIST_DIR=%ROOT%dist"
set "EXENAME=PolyHostInterface.exe"
set "FINAL_EXE=%DIST_DIR%\%EXENAME%"
set "VST2_SDK=%TOOLS%\vstsdk2.4"

echo.
echo ============================================================
echo PolyHost - Standalone App - Build Script
echo ============================================================
echo.

if not exist "%CMAKE%" (
    echo ERROR: cmake.exe not found at:
    echo %CMAKE%
    echo.
    echo Download the Windows x64 ZIP from:
    echo https://cmake.org/download/
    echo Extract it so this file exists:
    echo ..\_Tools\cmake\_4.4.2\bin\cmake.exe
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

if exist "%VST2_SDK%\pluginterfaces\vst2.x\aeffect.h" (
    echo VST2 SDK found - VST2 support will be compiled in.
) else (
    echo VST2 SDK not found at ..\_Tools\vstsdk2.4 - building without VST2.
    echo To add VST2, place this file here:
    echo ..\_Tools\vstsdk2.4\pluginterfaces\vst2.x\aeffect.h
)
echo.

cd /d "%ROOT%"

echo Closing any running instances of %EXENAME%...
taskkill /IM "%EXENAME%" /F 2>nul

echo Cleaning previous build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo.
echo Configuring project...
"%CMAKE%" -S "%ROOT%Source\APP" -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64
if errorlevel 1 (
    echo.
    echo Configure step FAILED.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo.
echo Building Release...
"%CMAKE%" --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo.
    echo Build FAILED.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

echo.
echo Copying standalone EXE to dist...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

if exist "%BUILD_DIR%\PolyHost_artefacts\Release\Standalone\%EXENAME%" (
    if exist "%FINAL_EXE%" del /f /q "%FINAL_EXE%"
    copy /Y "%BUILD_DIR%\PolyHost_artefacts\Release\Standalone\%EXENAME%" "%FINAL_EXE%" >nul
    if errorlevel 1 (
        echo ERROR: Failed to copy standalone EXE to:
        echo %FINAL_EXE%
        if not defined PHI_BUILD_CAPTURE pause
        exit /b 1
    )
    copy /Y "%ROOT%Source\FluentSystemIcons-LICENSE.txt" "%DIST_DIR%\FluentSystemIcons-LICENSE.txt" >nul
    if errorlevel 1 (
        echo ERROR: Failed to copy the Fluent System Icons licence to dist.
        if not defined PHI_BUILD_CAPTURE pause
        exit /b 1
    )
    echo Copied EXE to:
    echo %FINAL_EXE%
) else (
    echo Expected EXE not found at:
    echo %BUILD_DIR%\PolyHost_artefacts\Release\Standalone\%EXENAME%
    echo.
    echo Build may have succeeded with a different output path.
    if not defined PHI_BUILD_CAPTURE pause
    exit /b 1
)

:: Close cmd and launch app
echo.
echo ============================================================
echo APP Build complete. Launching %EXENAME% in 2 seconds...
echo ============================================================
timeout /t 2 /nobreak >nul
start "" "%FINAL_EXE%"
exit /b 0
