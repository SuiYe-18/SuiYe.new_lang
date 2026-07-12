@echo off
setlocal
cd /d "%~dp0\.."
if not exist build mkdir build
if not exist build\test mkdir build\test
if not exist build\reverse mkdir build\reverse
set SUIYE=bin\suiye.exe
set PATH=%CD%\bin;%PATH%

echo [test] version
%SUIYE% --version
if errorlevel 1 exit /b 1

echo [test] syntax
%SUIYE% --syntax
if errorlevel 1 exit /b 1

echo [test] core part1 ast check
%SUIYE% --check tests\core_part1_check.sye
if errorlevel 1 exit /b 1
%SUIYE% --check examples\all_syntax.sye
if errorlevel 1 exit /b 1

echo [test] core part1 value and scope
gcc -std=c11 -O2 -Wall -Wextra -Iinclude tests\core_part1_value_scope_test.c src\sye_value.c src\sye_scope.c -o build\test\core_part1_value_scope_test.exe
if errorlevel 1 exit /b 1
build\test\core_part1_value_scope_test.exe
if errorlevel 1 exit /b 1

echo [test] C embed api
gcc -std=c11 -O2 -Wall -Wextra -Iinclude tests\embed_c_test.c -Lbin -lSuiYeEmbed -o build\test\embed_c_test.exe
if errorlevel 1 exit /b 1
build\test\embed_c_test.exe
if errorlevel 1 exit /b 1

echo [test] embed stress
gcc -std=c11 -O2 -Wall -Wextra -Iinclude tests\embed_stress_test.c -Lbin -lSuiYeEmbed -o build\test\embed_stress_test.exe
if errorlevel 1 exit /b 1
build\test\embed_stress_test.exe
if errorlevel 1 exit /b 1

echo [test] core part2 ast runtime
%SUIYE% --ast-run tests\core_part2_ast_runtime.sye
if errorlevel 1 exit /b 1

echo [test] core part3 ast runtime diagnostics
%SUIYE% --ast-run tests\core_part3_ast_runtime.sye
if errorlevel 1 exit /b 1

echo [test] ast dll dispatch
%SUIYE% --ast-run tests\dll_ast_dispatch.sye
if errorlevel 1 exit /b 1

echo [test] dll industrial baseline
%SUIYE% --ast-run tests\dll_industrial_baseline.sye
if errorlevel 1 exit /b 1

echo [test] native industrial modules
%SUIYE% --ast-run tests\native_industrial_modules.sye
if errorlevel 1 exit /b 1

echo [test] self research 100 percent
%SUIYE% --ast-run tests\self_research_100.sye
if errorlevel 1 exit /b 1

echo [test] high depth native modules
%SUIYE% --ast-run tests\high_depth_native.sye
if errorlevel 1 exit /b 1

echo [test] direct run
%SUIYE% --run examples\all_syntax.sye
if errorlevel 1 exit /b 1

echo [test] legacy run
%SUIYE% --legacy-run examples\round5_regression.sye
if errorlevel 1 exit /b 1

echo [test] pack
%SUIYE% --pack examples\all_syntax.sye all_syntax_test.exe
if errorlevel 1 exit /b 1

echo [test] packed executable
build\dist\all_syntax_test.exe
if errorlevel 1 exit /b 1

echo [test] reverse
%SUIYE% --reverse build\dist\all_syntax_test.exe build\reverse\restored_all_syntax_test.sye
if errorlevel 1 exit /b 1
if not exist build\reverse\restored_all_syntax_test.sye.reverse-report.json exit /b 1

echo [test] reverse refuses non-packed exe
%SUIYE% --reverse bin\suiye.exe build\reverse\should_not_exist.sye
if not errorlevel 1 exit /b 1

echo [test] pack v2 ast resources deps
%SUIYE% Pack_it_up.dll --ast --deps --add tests\pack_resource.txt --product SuiYeAST --version 2.0.0 --company SuiYeTeam -o tests\core_part3_ast_runtime.sye ast_pack_test.exe
if errorlevel 1 exit /b 1
build\dist\ast_pack_test.exe
if errorlevel 1 exit /b 1
%SUIYE% --reverse build\dist\ast_pack_test.exe build\reverse\restored_ast_pack_test.sye
if errorlevel 1 exit /b 1
if not exist build\reverse\restored_ast_pack_test.sye.reverse-report.json exit /b 1
if not exist build\reverse\restored_ast_pack_test.sye.resources\pack_resource.txt exit /b 1
if not exist build\reverse\restored_ast_pack_test.sye.deps\GUI.dll exit /b 1

echo [test] ok
exit /b 0
