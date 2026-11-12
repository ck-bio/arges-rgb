@echo off
cd /d "%~dp0"
gcc -Wall -o test_effects.exe src/test_effects.c && test_effects.exe ^
  && windres res/arges.rc -O coff -o arges_res.o ^
  && gcc -Wall -Os -s -municode -mwindows -o arges.exe src/arges.c src/settings.c arges_res.o -lsetupapi -lhid -lshell32 -lwtsapi32 -lcomdlg32 -lcomctl32 ^
  && echo Built arges.exe
pause
