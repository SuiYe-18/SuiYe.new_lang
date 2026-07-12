cmake_minimum_required(VERSION 3.20)
project(SuiYe C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(include)
option(SUIYE_BUILD_WINDOWS_MODULES "Build Windows-specific SuiYe DLL ecosystem modules" ON)
if(NOT WIN32)
    set(SUIYE_BUILD_WINDOWS_MODULES OFF)
endif()

if(WIN32)
    set(SUIYE_CLI_SOURCE src/suiye.c)
else()
    set(SUIYE_CLI_SOURCE src/suiye_cli.c)
endif()

add_executable(suiye
    ${SUIYE_CLI_SOURCE}
    src/suiye_crash.c
    src/suiye_platform.c
    src/sye_lexer.c
    src/sye_ast.c
    src/sye_value.c
    src/sye_scope.c
    src/sye_runtime.c
)

target_link_libraries(suiye m)

add_library(SuiYeEmbed SHARED
    src/suiye_embed.c
    src/suiye_platform.c
    src/sye_lexer.c
    src/sye_ast.c
    src/sye_value.c
    src/sye_scope.c
    src/sye_runtime.c
)
target_compile_definitions(SuiYeEmbed PRIVATE SUIYE_EMBED_BUILD)
target_link_libraries(SuiYeEmbed m)
if(UNIX)
    target_link_libraries(suiye dl)
    target_link_libraries(SuiYeEmbed dl)
endif()

function(add_suiye_eco name)
    add_library(${name} SHARED modules/eco_module.c)
    target_compile_definitions(${name} PRIVATE SUIYE_BUILD_DLL MODULE_NAME=${name})
endfunction()

function(add_suiye_native name)
    add_library(${name} SHARED modules/native_industrial.c)
    target_compile_definitions(${name} PRIVATE SUIYE_BUILD_DLL MODULE_NAME=${name})
    target_link_libraries(${name} ws2_32)
endfunction()

if(SUIYE_BUILD_WINDOWS_MODULES)
    add_library(Pack_it_up SHARED modules/pack_it_up.c)
    target_compile_definitions(Pack_it_up PRIVATE SUIYE_BUILD_DLL)

    add_library(Reverse SHARED modules/reverse.c)
    target_compile_definitions(Reverse PRIVATE SUIYE_BUILD_DLL)

    add_library(SSLCertificate SHARED modules/ssl_certificate.c)
    target_compile_definitions(SSLCertificate PRIVATE SUIYE_BUILD_DLL)

    foreach(name GUI Icon_icos Math String FileSystem Json Xml Thread Process DateTime Regex Image Csv Yaml Console Terminal Memory SystemInfo Win32 Clipboard Chart DataFrame SerialPort)
        add_suiye_eco(${name})
    endforeach()

    foreach(name Network Http Sqlite Crypto Audio Video Archive WebView)
        add_suiye_native(${name})
    endforeach()
endif()
