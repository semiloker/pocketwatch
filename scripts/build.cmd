@echo off
rem Runs build.ps1 without changing the system-wide PowerShell execution policy.
rem Usage: scripts\build.cmd [-Target all|check|clean] [-Release] [-Distro Ubuntu]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
exit /b %ERRORLEVEL%
