@echo off
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\build.ps1" %*
if errorlevel 1 (
  echo Build failed. See the logs folder.
  pause
  exit /b 1
)
pause
