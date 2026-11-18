:: This script allows running the build batch file in ColorConsole
:: Set path to ColorConsole

@echo off
setlocal

set "ROOT=%~dp0"
set "COLORCONSOLE=D:\SyMenu\ProgramFiles\SPSSuite\SyMenuSuite\ColorConsole_sps\ColorConsole_Portable.exe"
set "BUILD_BAT=%ROOT%build-VST3.bat"

if not exist "%COLORCONSOLE%" (
    echo ERROR: ColorConsole.exe not found at:
    echo %COLORCONSOLE%
    echo.
    pause
    exit /b 1
)

if not exist "%BUILD_BAT%" (
    echo ERROR: build-VST3.bat not found at:
    echo %BUILD_BAT%
    echo.
    pause
    exit /b 1
)

start "" "%COLORCONSOLE%" "%BUILD_BAT%"
exit /b 0