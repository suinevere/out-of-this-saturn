:; set -eu
:; cd "$(dirname "$0")"
:;
:; DISC_NAME="Out of this World (USA)"
:; DATA_SRC="./(put bank and memlist files here)"
:; BASE_ISO="./bin/$DISC_NAME.iso"
:; TMP_DIR=./tmp
:; OUTPUT_DIR="./$DISC_NAME"
:;
:; mkdir -p "$TMP_DIR" "$DATA_SRC"
:;
:; UNAME_S=$(uname -s)
:; ensure_xorriso() {
:;     if command -v xorriso >/dev/null 2>&1; then return 0; fi
:;     INSTALL_CMD=""
:;     if [ "$UNAME_S" = Darwin ]; then
:;         if command -v brew >/dev/null 2>&1; then INSTALL_CMD="brew install xorriso"; fi
:;     elif command -v apt-get >/dev/null 2>&1; then INSTALL_CMD="sudo apt-get install -y xorriso"
:;     elif command -v dnf >/dev/null 2>&1; then INSTALL_CMD="sudo dnf install -y xorriso"
:;     elif command -v pacman >/dev/null 2>&1; then INSTALL_CMD="sudo pacman -S --noconfirm xorriso"
:;     elif command -v zypper >/dev/null 2>&1; then INSTALL_CMD="sudo zypper install -y xorriso"
:;     fi
:;     echo "xorriso is not installed, and building the disc needs it."
:;     if [ -z "$INSTALL_CMD" ]; then
:;         echo "Install xorriso with your package manager, then run this again." >&2
:;         return 1
:;     fi

:;     if [ ! -t 0 ]; then
:;         echo "Install it with: $INSTALL_CMD" >&2
:;         return 1
:;     fi

:;     printf 'Install it now with "%s"? [y/N] ' "$INSTALL_CMD"
:;     read -r REPLY || REPLY=n
:;     AGREED=0
:;     case "$REPLY" in y|Y|yes|Yes|YES) AGREED=1 ;; esac
:;     if [ "$AGREED" -ne 1 ]; then echo "Not installing. Run that yourself, then run this again." >&2; return 1; fi
:;     echo "Running: $INSTALL_CMD"

:;     if ! $INSTALL_CMD; then echo "That did not work. Install xorriso yourself, then run this again." >&2; return 1; fi
:;     if ! command -v xorriso >/dev/null 2>&1; then echo "xorriso still is not on PATH." >&2; return 1; fi
:;     echo "xorriso installed."

:;     return 0
:; }
:;
:; if [ "$UNAME_S" = Linux ] || [ "$UNAME_S" = Darwin ]; then ensure_xorriso || exit 1; fi
:;
:; echo "== Step 1/2: install your game data =="
:; ARC=$(find "$DATA_SRC" -maxdepth 1 -type f \( -iname '*.7z' -o -iname '*.zip' \) 2>/dev/null | sort | head -1)
:; if [ -n "$ARC" ]; then
:;     echo "Unpacking $(basename "$ARC")"
:;     if command -v unzip >/dev/null 2>&1; then unzip -qo "$ARC" -d "$DATA_SRC"
:;     elif command -v 7z >/dev/null 2>&1; then 7z x -y -o"$DATA_SRC" "$ARC" >/dev/null
:;     elif command -v bsdtar >/dev/null 2>&1; then bsdtar -xf "$ARC" -C "$DATA_SRC"
:;     else echo "ERROR: need unzip, 7z or bsdtar to unpack $ARC" >&2; exit 1; fi
:; fi
:;
:; SRCLIST="$TMP_DIR/.srclist"
:; find "$DATA_SRC" -type f \( -iname 'bank??' -o -iname 'memlist.bin' \) 2>/dev/null | sort > "$SRCLIST"
:; n=0
:; while IFS= read -r f; do b=$(basename "$f" | tr 'A-Z' 'a-z'); cp -f "$f" "$TMP_DIR/$b"; echo "  -> $b"; n=$((n + 1)); done < "$SRCLIST"
:; rm -f "$SRCLIST"
:;
:; if [ "$n" -ne 14 ]; then
:;     echo "ERROR: expected 14 data files, found $n in" >&2
:;     echo "  $DATA_SRC" >&2
:;     echo "" >&2
:;     echo "Put your own copy of the Out of this World PC DOS" >&2
:;     echo "release in that folder -- bank01 through bank0d and memlist.bin, loose" >&2
:;     echo "or still in a .zip. This kit never downloads game data." >&2
:;     exit 1
:; fi
:; echo "Installed $n files into $TMP_DIR"
:;
:; echo "== Step 2/2: build the disc =="
:; [ -f "$BASE_ISO" ] || { echo "ERROR: the kit's disc image is missing: $BASE_ISO" >&2; exit 1; }
:; . bin/inject.sh
:; inject_data "$BASE_ISO" "$TMP_DIR" "$OUTPUT_DIR" "$DISC_NAME"
:;
:; rm -rf "$TMP_DIR"
:;
:; echo
:; echo "Ready to burn or mount: $OUTPUT_DIR/$DISC_NAME.cue"
:; exit

@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
CD /D "%~dp0"

SET "DISC_NAME=Out of this World (USA)"
SET "DATA_SRC=(put bank and memlist files here)"
SET "BASE_ISO=bin\%DISC_NAME%.iso"
SET "TMP_DIR=tmp"
SET "OUTPUT_DIR=%DISC_NAME%"

IF NOT EXIST "%TMP_DIR%" MKDIR "%TMP_DIR%"
IF NOT EXIST "%DATA_SRC%" MKDIR "%DATA_SRC%"

ECHO == Step 1/2: install your game data ==
SET "ARC="
FOR %%F IN ("%DATA_SRC%\*.7z" "%DATA_SRC%\*.zip") DO IF NOT DEFINED ARC SET "ARC=%%~fF"
IF DEFINED ARC CALL :unpack

SET /A N=0
FOR /R "%DATA_SRC%" %%F IN (bank?? memlist.bin) DO CALL :install "%%~fF" "%%~nxF"

IF NOT "!N!"=="14" (
    ECHO ERROR: expected 14 data files, found !N! in
    ECHO   "%DATA_SRC%"
    ECHO.
    ECHO Put your own copy of the Out of this World PC DOS
    ECHO release in that folder -- bank01 through bank0d and memlist.bin, loose
    ECHO or still in a .zip. This kit never downloads game data.
    EXIT /B 1
)
ECHO Installed !N! files into "%TMP_DIR%"

ECHO == Step 2/2: build the disc ==
IF NOT EXIST "%BASE_ISO%" ( ECHO ERROR: the kit's disc image is missing: "%BASE_ISO%" & EXIT /B 1 )
powershell -NoProfile -ExecutionPolicy Bypass -File ".\bin\inject.ps1" -BaseIso "%BASE_ISO%" -DataDir "%TMP_DIR%" -OutDir "%OUTPUT_DIR%" -Name "%DISC_NAME%" -Xorriso ".\bin\win\xorriso.exe" -Iso2raw ".\bin\win\iso2raw.exe"
IF ERRORLEVEL 1 ( ECHO ERROR: disc build failed & EXIT /B 1 )

RMDIR /S /Q "%TMP_DIR%" 2>NUL

ECHO.
ECHO Ready to burn or mount: "%OUTPUT_DIR%\%DISC_NAME%.cue"
ENDLOCAL
EXIT /B 0

:unpack
ECHO Unpacking "!ARC!"
powershell -NoProfile -Command "Expand-Archive -LiteralPath '!ARC!' -DestinationPath '%DATA_SRC%' -Force"
IF ERRORLEVEL 1 ( ECHO Could not unpack "!ARC!" -- a .7z needs 7-Zip, or unpack it yourself. )
GOTO :eof

:install
powershell -NoProfile -Command "$n='%~2'.ToLower(); Copy-Item -LiteralPath '%~1' -Destination (Join-Path '%TMP_DIR%' $n) -Force"
ECHO   -^> %~2
SET /A N+=1
GOTO :eof
