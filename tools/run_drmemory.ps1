param(
  [string]$DrMemory = "drmemory.exe"
)

$ErrorActionPreference = "Stop"
& .\build.bat release
& $DrMemory -batch -exit_code_if_errors 42 -- .\bin\suiye.exe --run .\examples\hello.sye
& $DrMemory -batch -exit_code_if_errors 42 -- .\bin\suiye.exe --run .\tests\core_part3_ast_runtime.sye
Write-Host "[OK] Dr.Memory passed"
