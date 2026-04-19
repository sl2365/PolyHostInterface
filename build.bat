@echo off
:: ============================================================
:: PolyHost Build Script - Fully Portable
:: ============================================================

set "ROOT=%~dp0"
set "CMAKE=%ROOT%tools\cmake\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build"
set "DIST_DIR=%ROOT%dist"
set "EXENAME=PolyHost.exe"

if not exist "%CMAKE%" (
    echo ERROR: cmake.exe not found at: %CMAKE%
    echo Download the Windows x64 ZIP from https://cmake.org/download/
    echo Extract so that tools\cmake\bin\cmake.exe exists.
    pause
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio 2022 not found.
    echo Install Community edition with Desktop development with C++ workload.
    echo https://visualstudio.microsoft.com/vs/community/
    pause
    exit /b 1
)

cd /d "%ROOT%"
echo Closing any running instances of %EXENAME%...
taskkill /IM "%EXENAME%" /F 2>nul
echo Cleaning previous build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
echo Configuring (first run downloads JUCE -- needs internet)...
"%CMAKE%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 ( echo Configure FAILED. & pause & exit /b 1 )
echo Building Release...
"%CMAKE%" --build "%BUILD_DIR%" --config Release
if %ERRORLEVEL% NEQ 0 ( echo Build FAILED. & pause & exit /b 1 )
echo Build successful! Copying to dist\...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
copy /Y "%BUILD_DIR%\PolyHost_artefacts\Release\%EXENAME%" "%DIST_DIR%\%EXENAME%"
echo Starting %EXENAME%...
timeout /t 2 /nobreak >nul
start "" "%DIST_DIR%\%EXENAME%"
exit
