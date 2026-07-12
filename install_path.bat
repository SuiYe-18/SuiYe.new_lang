@echo off
setlocal
set TARGET=%~dp0bin
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p=[Environment]::GetEnvironmentVariable('Path','User'); $t='%TARGET:~0,-1%'; if(($p -split ';') -notcontains $t){[Environment]::SetEnvironmentVariable('Path', ($p.TrimEnd(';')+';'+$t), 'User'); Write-Host '[OK] SuiYe bin added to user PATH' -ForegroundColor Green}else{Write-Host '[OK] SuiYe bin already in user PATH' -ForegroundColor Green}"
echo 请重新打开终端后使用 suiye 命令。
