@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0run-quality-audit.ps1" %*
exit /b %ERRORLEVEL%