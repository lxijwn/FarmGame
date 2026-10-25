@echo off
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\verify.ps1" %*
if errorlevel 1 (
  echo Verification failed. Check Validation\latest-verification.json and logs.
  pause
  exit /b 1
)
pause
