:: Build the completely isolated 32-bit standalone comparison host.
:: This script does not use or modify the existing PHI64 build folders.
:: Requires Visual Studio 2026 with Desktop development with C++
:: and the MSVC x86 build tools installed.

@echo off
setlocal

set "ROOT=%~dp0"
for %%I in ("%ROOT%..") do set "PROJECTS_ROOT=%%~fI"
set "TOOLS=%PROJECTS_ROOT%\_Tools"
set "CMAKE=%TOOLS%\cmake\_4.4.2\bin\cmake.exe"
set "BUILD_DIR=%ROOT%build-APP-32"
set "DIST_DIR=%ROOT%dist-32"
set "EXENAME=PolyHostInterface32.exe"
set "FINAL_EXE=%DIST_DIR%\%EXENAME%"
set "VST2_SDK=%TOOLS%\vstsdk2.4"

echo.
echo ============================================================
echo PolyHost - Isolated 32-bit Standalone Comparison Host
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
    pause
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio Build Tools not found.
    echo Install Visual Studio 2026 Community or Build Tools.
    echo Make sure Desktop development with C++ and the MSVC x86 tools are installed.
    echo.
    pause
    exit /b 1
)

if exist "%VST2_SDK%\pluginterfaces\vst2.x\aeffect.h" (
    echo VST2 SDK found - 32-bit VST2 hosting will be compiled in.
) else (
    echo ERROR: VST2 SDK not found at ..\_Tools\vstsdk2.4
    echo PHI32 requires this file to host VST2.4 plugins:
    echo ..\_Tools\vstsdk2.4\pluginterfaces\vst2.x\aeffect.h
    echo.
    pause
    exit /b 1
)
echo.

cd /d "%ROOT%"

echo Closing any running instances of %EXENAME%...
taskkill /IM "%EXENAME%" /F 2>nul

echo Cleaning previous isolated 32-bit build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo.
echo Configuring isolated 32-bit project...
"%CMAKE%" -S "%ROOT%Source\APP32" -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A Win32
if errorlevel 1 (
    echo.
    echo Configure step FAILED.
    echo Confirm the MSVC x86 build tools are installed in Visual Studio.
    pause
    exit /b 1
)

echo.
echo Building 32-bit Release...
"%CMAKE%" --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo.
    echo Build FAILED.
    pause
    exit /b 1
)

echo.
echo Copying PHI32 standalone EXE to dist-32...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

if exist "%BUILD_DIR%\PolyHost32_artefacts\Release\Standalone\%EXENAME%" (
    if exist "%FINAL_EXE%" del /f /q "%FINAL_EXE%"
    copy /Y "%BUILD_DIR%\PolyHost32_artefacts\Release\Standalone\%EXENAME%" "%FINAL_EXE%" >nul
    if errorlevel 1 (
        echo ERROR: Failed to copy PHI32 standalone EXE to:
        echo %FINAL_EXE%
        pause
        exit /b 1
    )
    echo Copied PHI32 EXE to:
    echo %FINAL_EXE%
) else (
    echo Expected PHI32 EXE not found at:
    echo %BUILD_DIR%\PolyHost32_artefacts\Release\Standalone\%EXENAME%
    echo.
    echo Build may have succeeded with a different output path.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo PHI32 build complete. Launching %EXENAME% in 2 seconds...
echo ============================================================
timeout /t 2 /nobreak >nul
start "" "%FINAL_EXE%"
exit /b 0
