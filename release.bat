@echo off
setlocal
if not exist bin\suiye.exe (
  call build.bat release
  if errorlevel 1 exit /b 1
)
if exist build\release rmdir /s /q build\release
mkdir build\release
xcopy /e /i /y bin build\release\bin >nul
copy /y README.md build\release\ >nul
copy /y CHANGELOG.md build\release\ >nul
copy /y VERSION build\release\ >nul
copy /y install_path.bat build\release\ >nul
copy /y quality_gate.bat build\release\ >nul
if exist docs xcopy /e /i /y docs build\release\docs >nul
if exist embeddings xcopy /e /i /y embeddings build\release\embeddings >nul
if exist tools xcopy /e /i /y tools build\release\tools >nul
if exist vscode xcopy /e /i /y vscode build\release\vscode >nul
powershell -NoProfile -Command "Write-Host '[release] complete: build\release' -ForegroundColor Green"
exit /b 0
