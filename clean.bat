@echo off
setlocal
powershell -NoProfile -Command "Write-Host '[clean] removing generated build outputs' -ForegroundColor Cyan"
if exist bin rmdir /s /q bin
if exist build\dist rmdir /s /q build\dist
if exist build\log rmdir /s /q build\log
if exist build\reverse rmdir /s /q build\reverse
if exist build\test rmdir /s /q build\test
if exist build\release rmdir /s /q build\release
if exist tests\*.exe del /q tests\*.exe
if exist tests\*.suidb del /q tests\*.suidb
if exist tests\*.sya del /q tests\*.sya
if exist tests\*.csv del /q tests\*.csv
if exist tests\*.ico del /q tests\*.ico
if exist tests\*.svg del /q tests\*.svg
if exist tests\*.suicert del /q tests\*.suicert
if exist tests\*.html del /q tests\*.html
if exist tests\*.wav del /q tests\*.wav
if exist *.dll del /q *.dll
if exist suiye.exe del /q suiye.exe
if not exist bin mkdir bin
if not exist build mkdir build
if not exist build\dist mkdir build\dist
if not exist build\log mkdir build\log
if not exist build\reverse mkdir build\reverse
if not exist build\test mkdir build\test
powershell -NoProfile -Command "Write-Host '[clean] complete' -ForegroundColor Green"
