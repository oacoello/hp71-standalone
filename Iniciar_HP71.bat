@echo off
setlocal
cd /d "%~dp0"
start "" wscript.exe "%~dp0Iniciar_HP71.vbs" %*
