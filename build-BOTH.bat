@echo off
setlocal EnableExtensions

if /i "%~1"=="--run-build" goto :run_build

set "RESULTS_LOG=%~dp0Results.log"
set "PHI_BUILD_SCRIPT=%~f0"
set "PHI_BUILD_CAPTURE=1"

echo Starting all PHI builds...
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

call "%~dp0build-VST3.bat" --run-build
if errorlevel 1 exit /b 1

call "%~dp0build-APP.bat" --run-build
if errorlevel 1 exit /b 1

call "%~dp0build-APP-32.bat" --run-build
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo All PHI builds completed successfully.
echo ============================================================
exit /b 0
