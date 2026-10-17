@echo off
rem Runs deploy.ps1 without changing the system-wide PowerShell execution policy.
rem Usage: scripts\deploy.cmd [-Drive E] [-Enable] [-EnableXmb] [-Device ms0|ef0]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy.ps1" %*
exit /b %ERRORLEVEL%
