@echo off
call build-VST3.bat
if errorlevel 1 exit /b 1

call build-APP.bat
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo Both builds completed successfully.
echo ============================================================
pause