#!/bin/sh
set -e
cd "$(dirname "$0")"

ENGINE_FLAGS="-std=c++11 -Wall -O1 -g"
OWN_FLAGS="-std=c++11 -Wall -Wextra -Werror -O1 -g"
INC="$(find ../src -type d | sed 's/^/-I/' | tr '\n' ' ')"

echo "== scsp_voice =="
g++ $OWN_FLAGS $INC \
    -o run_tests_scsp test_scsp_voice.cxx ../src/system/scsp_voice.cxx
./run_tests_scsp

echo "== cdda_classify =="
g++ $OWN_FLAGS $INC \
    -o run_tests_cdda_classify test_cdda_classify.cxx ../src/sound/cdda_classify.c
./run_tests_cdda_classify

echo "== savefmt =="
g++ $ENGINE_FLAGS -DAUTO_DETECT_PLATFORM $INC \
    -o run_tests_savefmt test_savefmt.cxx \
    ../src/engine/serializer.cxx ../src/engine/file.cxx ../src/engine/util.cxx
./run_tests_savefmt

echo "== backup date =="
g++ $OWN_FLAGS $INC \
    -o run_tests_bupdate test_backup_date.cxx stub_backup.cxx
./run_tests_bupdate

echo "== backup stub =="
g++ $OWN_FLAGS $INC \
    -o run_tests_bupstub test_backup_stub.cxx stub_backup.cxx
./run_tests_bupstub

echo "== savedata =="
g++ $OWN_FLAGS -DAUTO_DETECT_PLATFORM $INC \
    -o run_tests_savedata test_savedata.cxx stub_backup.cxx \
    ../src/save/savedata.cxx
./run_tests_savedata

echo "== bup devmap =="
g++ $OWN_FLAGS $INC \
    -o run_tests_bupdevmap test_bup_devmap.cxx ../src/system/bup_devmap.cxx
./run_tests_bupdevmap

echo "== keymap =="
g++ $OWN_FLAGS $INC \
    -o run_tests_keymap test_keymap.cxx ../src/input/keymap.cxx
./run_tests_keymap

echo "== settings =="
g++ $OWN_FLAGS $INC \
    -o run_tests_settings test_settings.cxx stub_backup.cxx \
    ../src/save/settings.cxx ../src/input/keymap.cxx
./run_tests_settings

echo "== checkpoints =="
g++ $OWN_FLAGS $INC \
    -o run_tests_checkpoints test_checkpoints.cxx ../src/engine/checkpoints.cxx
./run_tests_checkpoints

echo "== menu state =="
g++ $OWN_FLAGS -DAUTO_DETECT_PLATFORM $INC \
    -o run_tests_menustate test_menu_state.cxx stub_backup.cxx \
    ../src/menus/menu_state.cxx ../src/save/savedata.cxx ../src/input/keymap.cxx \
    ../src/engine/checkpoints.cxx
./run_tests_menustate

echo "== menu draw =="
g++ $OWN_FLAGS $INC \
    -o run_tests_menudraw test_menu_draw.cxx ../src/menus/menu_draw.cxx
./run_tests_menudraw

echo "== menu art =="
g++ $OWN_FLAGS $INC \
    -o run_tests_menuart test_menu_art.cxx ../src/menus/menu_draw.cxx \
    ../src/menus/menu_blit.cxx ../src/menus/menu_art.cxx
./run_tests_menuart

echo "== page rle =="
g++ $OWN_FLAGS $INC \
    -o run_tests_pagerle test_page_rle.cxx ../src/video/page_rle.cxx
./run_tests_pagerle

echo "== cdtoc =="
g++ $OWN_FLAGS $INC -o run_tests_cdtoc test_cdtoc.cxx ../src/sound/cdtoc.c
./run_tests_cdtoc

echo "== cdda_gate =="
g++ $OWN_FLAGS $INC \
    -o run_tests_cdda_gate test_cdda_gate.cxx ../src/sound/cdda_gate.c
./run_tests_cdda_gate

echo "== part_music =="
g++ $OWN_FLAGS -DAUTO_DETECT_PLATFORM $INC \
    -o run_tests_part_music test_part_music.cxx ../src/sound/part_music.cxx ../src/engine/checkpoints.cxx ../src/engine/parts.cxx
./run_tests_part_music

echo "== cdda_want =="
g++ $OWN_FLAGS $INC \
    -o run_tests_cdda_want test_cdda_want.cxx ../src/sound/cdda_want.c
./run_tests_cdda_want

echo "== cdda_arm =="
g++ $OWN_FLAGS $INC \
    -o run_tests_cdda_arm test_cdda_arm.cxx ../src/sound/cdda_arm.c
./run_tests_cdda_arm

echo "== cue_index =="
g++ $OWN_FLAGS -DAUTO_DETECT_PLATFORM $INC \
    -o run_tests_cue_index test_cue_index.cxx ../src/sound/cue_index.cxx
./run_tests_cue_index

echo "== cd_music =="
g++ $OWN_FLAGS -DAUTO_DETECT_PLATFORM $INC \
    -o run_tests_cd_music test_cd_music.cxx ../src/sound/cd_music.cxx \
    ../src/sound/part_music.cxx ../src/engine/checkpoints.cxx ../src/engine/parts.cxx \
    ../src/sound/cdda_arm.c ../src/sound/cdda_classify.c ../src/sound/cdda_gate.c \
    ../src/sound/cdda_want.c ../src/sound/cdtoc.c
./run_tests_cd_music

echo "all suites passed"
