:; set -eu
:; is_url() { case "$1" in http://*|https://*) return 0 ;; *) return 1 ;; esac; }
:; SRC=${1:-}
:; if [ -n "$SRC" ] && ! is_url "$SRC"; then case "$SRC" in /*) ;; *) SRC="$PWD/$SRC" ;; esac; fi
:; cd "$(dirname "$0")"
:;
:; cfg() { (sed -n "s/^$1=//p" CONFIG.ME 2>/dev/null || true) | head -1 | tr -d '\r'; }
:; GAME_URL=$(cfg GAME_URL)
:; GAME_MD5=$(cfg GAME_MD5)
:; DATA_DIR=$(cfg DATA_DIR)
:; DATA_DIR=${DATA_DIR:-../../saturn/cd/data}
:; mkdir -p "$DATA_DIR"
:; DEST=$(cd "$DATA_DIR" && pwd)
:;
:; if [ -z "$SRC" ]; then
:;     if [ -f "$DEST/memlist.bin" ]; then echo "Data already installed in $DEST."; exit 0; fi
:;     SRC=$GAME_URL
:; fi
:; if [ -z "$SRC" ]; then
:;     echo "Usage: data.bat PATH|URL" >&2
:;     echo "PATH is a folder or .zip holding your PC DOS copy of Out of this World:" >&2
:;     echo "bank01 through bank0d and memlist.bin. URL is a .zip to download." >&2
:;     exit 1
:; fi
:;
:; tmp=$(mktemp -d)
:; trap 'rm -rf "$tmp"' EXIT INT TERM
:; if is_url "$SRC"; then
:;     echo "Downloading $SRC"
:;     if command -v curl >/dev/null 2>&1; then curl -fL --retry 3 -o "$tmp/game.zip" "$SRC" || true
:;     elif command -v wget >/dev/null 2>&1; then wget -q -O "$tmp/game.zip" "$SRC" || true
:;     else echo "ERROR: need curl or wget on PATH" >&2; exit 1; fi
:;     [ -s "$tmp/game.zip" ] || { echo "ERROR: could not download $SRC" >&2; exit 1; }
:;     if [ -n "$GAME_MD5" ] && command -v md5sum >/dev/null 2>&1; then
:;         sum=$(md5sum "$tmp/game.zip" | cut -d' ' -f1)
:;         [ "$sum" = "$GAME_MD5" ] || { echo "ERROR: archive md5 $sum does not match GAME_MD5 $GAME_MD5" >&2; exit 1; }
:;     fi
:;     SRC=$tmp/game.zip
:; fi
:; [ -e "$SRC" ] || { echo "ERROR: $SRC does not exist" >&2; exit 1; }
:;
:; LOOK=$SRC
:; if [ -f "$SRC" ]; then
:;     mkdir -p "$tmp/x"
:;     if command -v unzip >/dev/null 2>&1; then unzip -qo "$SRC" -d "$tmp/x"
:;     elif command -v bsdtar >/dev/null 2>&1; then bsdtar -xf "$SRC" -C "$tmp/x"
:;     elif command -v python3 >/dev/null 2>&1; then python3 -c 'import sys,zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' "$SRC" "$tmp/x"
:;     else echo "ERROR: need unzip, bsdtar or python3 on PATH" >&2; exit 1; fi
:;     LOOK=$tmp/x
:; fi
:;
:; find "$LOOK" -type f \( -iname 'bank??' -o -iname 'memlist.bin' \) | sort > "$tmp/list"
:; n=0
:; while IFS= read -r f; do b=$(basename "$f" | tr 'A-Z' 'a-z'); cp -f "$f" "$DEST/$b"; echo "  -> $b"; n=$((n + 1)); done < "$tmp/list"
:; [ "$n" -eq 14 ] || { echo "ERROR: expected 14 data files (13 banks + memlist.bin), found $n" >&2; exit 1; }
:;
:; echo "Installed $n files into $DEST"
:; exit

@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
SET "SRC=%~1"
IF NOT DEFINED SRC GOTO :nosrc
IF /I "%SRC:~0,7%"=="http://" GOTO :nosrc
IF /I "%SRC:~0,8%"=="https://" GOTO :nosrc
SET "SRC=%~f1"
:nosrc
CD /D "%~dp0"

SET "GAME_URL="
SET "GAME_MD5="
SET "DATA_DIR="
IF EXIST "CONFIG.ME" FOR /F "usebackq eol=# tokens=1,* delims==" %%A IN ("CONFIG.ME") DO (
    IF "%%A"=="GAME_URL" SET "GAME_URL=%%B"
    IF "%%A"=="GAME_MD5" SET "GAME_MD5=%%B"
    IF "%%A"=="DATA_DIR" SET "DATA_DIR=%%B"
)
IF NOT DEFINED DATA_DIR SET "DATA_DIR=../../saturn/cd/data"
SET "DATA_DIR=%DATA_DIR:/=\%"
IF NOT EXIST "%DATA_DIR%" MKDIR "%DATA_DIR%"
FOR %%I IN ("%DATA_DIR%") DO SET "DEST=%%~fI"

IF DEFINED SRC GOTO :havesrc
IF EXIST "%DEST%\memlist.bin" (
    ECHO Data already installed in "%DEST%".
    EXIT /B 0
)
SET "SRC=%GAME_URL%"
IF DEFINED SRC GOTO :havesrc
ECHO Usage: data.bat PATH^|URL
ECHO PATH is a folder or .zip holding your PC DOS copy of Out of this World:
ECHO bank01 through bank0d and memlist.bin. URL is a .zip to download.
EXIT /B 1

:havesrc
SET "TMP_DIR=%TEMP%\ootw_data"
SET "TMP_ZIP=%TEMP%\ootw_data.zip"
IF /I "%SRC:~0,7%"=="http://" GOTO :download
IF /I "%SRC:~0,8%"=="https://" GOTO :download
GOTO :local

:download
ECHO Downloading %SRC%
curl -fL --retry 3 -o "%TMP_ZIP%" "%SRC%"
IF ERRORLEVEL 1 ( ECHO ERROR: could not download %SRC% & EXIT /B 1 )
IF NOT DEFINED GAME_MD5 GOTO :downloaded
powershell -NoProfile -Command "$got=(Get-FileHash -LiteralPath '%TMP_ZIP%' -Algorithm MD5).Hash.ToLower(); if ($got -ne '%GAME_MD5%') { Write-Host ('ERROR: archive md5 ' + $got + ' does not match GAME_MD5 %GAME_MD5%'); exit 1 }"
IF ERRORLEVEL 1 EXIT /B 1
:downloaded
SET "SRC=%TMP_ZIP%"

:local
IF NOT EXIST "%SRC%" ( ECHO ERROR: "%SRC%" does not exist & EXIT /B 1 )
SET "LOOK=%SRC%"
IF EXIST "%SRC%\*" GOTO :copy
IF EXIST "%TMP_DIR%" RMDIR /S /Q "%TMP_DIR%"
MKDIR "%TMP_DIR%"
powershell -NoProfile -Command "Expand-Archive -LiteralPath '%SRC%' -DestinationPath '%TMP_DIR%' -Force"
IF ERRORLEVEL 1 ( ECHO ERROR: extract failed & EXIT /B 1 )
SET "LOOK=%TMP_DIR%"

:copy
powershell -NoProfile -Command "$n=0; Get-ChildItem -LiteralPath '%LOOK%' -Recurse -File | Where-Object { $_.Name -match '^(bank[0-9a-f]{2}|memlist\.bin)$' } | ForEach-Object { $t = $_.Name.ToLower(); Copy-Item -LiteralPath $_.FullName -Destination (Join-Path '%DEST%' $t) -Force; Write-Host ('  -> ' + $t); $n++ }; if ($n -ne 14) { Write-Host ('ERROR: expected 14 data files (13 banks + memlist.bin), found ' + $n); exit 1 }; Write-Host ('Installed ' + $n + ' files into ' + '%DEST%')"
IF ERRORLEVEL 1 ( SET "RC=1" ) ELSE ( SET "RC=0" )
IF EXIST "%TMP_DIR%" RMDIR /S /Q "%TMP_DIR%"
IF EXIST "%TMP_ZIP%" DEL /Q "%TMP_ZIP%"
ENDLOCAL & EXIT /B %RC%
