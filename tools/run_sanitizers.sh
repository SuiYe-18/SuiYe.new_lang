#!/usr/bin/env sh
set -eu
cmake -S . -B build/asan -DSUIYE_BUILD_WINDOWS_MODULES=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build/asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/suiye --run examples/hello.sye
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/suiye --run tests/core_part3_ast_runtime.sye
echo "[OK] sanitizers passed"
