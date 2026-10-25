@echo off
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\run.ps1" %*
if errorlevel 1 (
  echo Launch failed. Build the project first.
  pause
  exit /b 1
)
