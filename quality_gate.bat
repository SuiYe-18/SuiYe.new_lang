@echo off
setlocal
echo [quality] build release
call build.bat release
if errorlevel 1 exit /b 1
echo [quality] full regression
call tests\run_all.bat
if errorlevel 1 exit /b 1
echo [quality] static analyzer sample
gcc -std=c11 -fanalyzer -Wall -Wextra -Iinclude src\sye_value.c src\sye_scope.c tests\core_part1_value_scope_test.c -o build\test\quality_analyzer_sample.exe
if errorlevel 1 exit /b 1
echo [quality] fuzz smoke
python tools\fuzz_suiye.py 25
if errorlevel 1 exit /b 1
echo [quality] short stress
python tools\long_stress.py --seconds 5 --script tests\core_part3_ast_runtime.sye
if errorlevel 1 exit /b 1
echo [OK] industrial quality gate passed
exit /b 0
