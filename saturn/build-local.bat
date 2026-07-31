@echo off
setlocal
cd /d "%~dp0"
set SRL_INSTALL_ROOT=../SaturnRingLib
call ..\SaturnRingLib\tools\scripts\make.bat release ../SaturnRingLib/Compiler %*
endlocal
