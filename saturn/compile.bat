:; export SRL_INSTALL_ROOT="${SRL_INSTALL_ROOT:-../SaturnRingLib}"; if [ "${1:-debug}" = "clean" ]; then make clean; elif [ "${1:-debug}" = "release" ]; then make all; else make all DEBUG=1; fi; exit;
@ECHO Off
SETLOCAL
IF "%~1"=="" (SET "TGT=debug") ELSE (SET "TGT=%~1")
IF NOT DEFINED SRL_INSTALL_ROOT SET "SRL_INSTALL_ROOT=../SaturnRingLib"
IF DEFINED SRL_COMPILER_DIR (SET "CDIR=%SRL_COMPILER_DIR%") ELSE (SET "CDIR=%~dp0..\SaturnRingLib\Compiler")
SET "PATH=%CDIR%\sh2eb-elf\bin;%CDIR%\msys2\usr\bin;%CDIR%\Other Utilities;%PATH%"
IF /I "%TGT%"=="clean"   GOTO doclean
IF /I "%TGT%"=="release" GOTO dorelease
make all DEBUG=1
GOTO done
:dorelease
make all
GOTO done
:doclean
make clean
GOTO done
:done
ENDLOCAL
