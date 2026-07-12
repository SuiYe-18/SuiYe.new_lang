@echo off
setlocal enabledelayedexpansion
set BUILD_MODE=%~1
if "%BUILD_MODE%"=="" set BUILD_MODE=release
if not exist bin mkdir bin
if not exist build mkdir build
if not exist build\dist mkdir build\dist
if not exist build\log mkdir build\log
set COMMON_WARN=-Wall -Wextra -Wno-cast-function-type -Wno-format-truncation -Wno-unused-function -Wno-misleading-indentation
set OPT_FLAGS=-O2
if /I "%BUILD_MODE%"=="debug" set OPT_FLAGS=-O0 -g -DDEBUG
if /I "%BUILD_MODE%"=="release" set OPT_FLAGS=-O2 -DNDEBUG
echo [build] mode: %BUILD_MODE%
echo [build] compiling suiye.exe
gcc -std=c11 %OPT_FLAGS% %COMMON_WARN% -Iinclude src\suiye.c src\suiye_crash.c src\suiye_platform.c src\sye_lexer.c src\sye_ast.c src\sye_value.c src\sye_scope.c src\sye_runtime.c -o bin\suiye.exe -lm
if errorlevel 1 exit /b 1
echo [build] compiling special dlls
gcc -std=c11 %OPT_FLAGS% %COMMON_WARN% -DSUIYE_EMBED_BUILD -shared -Iinclude src\suiye_embed.c src\suiye_platform.c src\sye_lexer.c src\sye_ast.c src\sye_value.c src\sye_scope.c src\sye_runtime.c -o bin\SuiYeEmbed.dll -Wl,--out-implib,bin\libSuiYeEmbed.dll.a -lm
if errorlevel 1 exit /b 1
gcc -std=c11 %OPT_FLAGS% %COMMON_WARN% -shared -Iinclude modules\pack_it_up.c -o bin\Pack_it_up.dll
if errorlevel 1 exit /b 1
gcc -std=c11 %OPT_FLAGS% %COMMON_WARN% -shared -Iinclude modules\reverse.c -o bin\Reverse.dll
if errorlevel 1 exit /b 1
gcc -std=c11 %OPT_FLAGS% %COMMON_WARN% -shared -Iinclude modules\ssl_certificate.c -o bin\SSLCertificate.dll
if errorlevel 1 exit /b 1
echo [build] compiling ecosystem dlls
set ECO_GCC=-std=c11 %OPT_FLAGS% %COMMON_WARN% -shared -Iinclude modules\eco_module.c
set NATIVE_GCC=-std=c11 %OPT_FLAGS% %COMMON_WARN% -shared -Iinclude modules\native_industrial.c -lws2_32
gcc %ECO_GCC% -o bin\GUI.dll -DMODULE_NAME=GUI
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Icon_icos.dll -DMODULE_NAME=Icon_icos
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Math.dll -DMODULE_NAME=Math
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\String.dll -DMODULE_NAME=String
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\FileSystem.dll -DMODULE_NAME=FileSystem
if errorlevel 1 exit /b 1
gcc %NATIVE_GCC% -o bin\Network.dll -DMODULE_NAME=Network
if errorlevel 1 exit /b 1
gcc %NATIVE_GCC% -o bin\Http.dll -DMODULE_NAME=Http
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Json.dll -DMODULE_NAME=Json
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Xml.dll -DMODULE_NAME=Xml
if errorlevel 1 exit /b 1
gcc %NATIVE_GCC% -o bin\Sqlite.dll -DMODULE_NAME=Sqlite
if errorlevel 1 exit /b 1
gcc %NATIVE_GCC% -o bin\Crypto.dll -DMODULE_NAME=Crypto
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Thread.dll -DMODULE_NAME=Thread
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Process.dll -DMODULE_NAME=Process
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\DateTime.dll -DMODULE_NAME=DateTime
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Regex.dll -DMODULE_NAME=Regex
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Image.dll -DMODULE_NAME=Image
if errorlevel 1 exit /b 1
gcc %NATIVE_GCC% -o bin\Audio.dll -DMODULE_NAME=Audio
if errorlevel 1 exit /b 1
gcc %NATIVE_GCC% -o bin\Video.dll -DMODULE_NAME=Video
if errorlevel 1 exit /b 1
gcc %NATIVE_GCC% -o bin\Archive.dll -DMODULE_NAME=Archive
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Csv.dll -DMODULE_NAME=Csv
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Yaml.dll -DMODULE_NAME=Yaml
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Console.dll -DMODULE_NAME=Console
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Terminal.dll -DMODULE_NAME=Terminal
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Memory.dll -DMODULE_NAME=Memory
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\SystemInfo.dll -DMODULE_NAME=SystemInfo
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Win32.dll -DMODULE_NAME=Win32
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Clipboard.dll -DMODULE_NAME=Clipboard
if errorlevel 1 exit /b 1
gcc %NATIVE_GCC% -o bin\WebView.dll -DMODULE_NAME=WebView
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\Chart.dll -DMODULE_NAME=Chart
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\DataFrame.dll -DMODULE_NAME=DataFrame
if errorlevel 1 exit /b 1
gcc %ECO_GCC% -o bin\SerialPort.dll -DMODULE_NAME=SerialPort
if errorlevel 1 exit /b 1
echo [OK] build complete: bin\suiye.exe and bin\*.dll compiled successfully
exit /b 0
