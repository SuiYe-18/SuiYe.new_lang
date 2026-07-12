#!/usr/bin/env sh
set -eu
cmake -S . -B build/valgrind -DSUIYE_BUILD_WINDOWS_MODULES=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build/valgrind
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=42 ./bin/suiye --run examples/hello.sye
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=42 ./bin/suiye --run tests/core_part3_ast_runtime.sye
echo "[OK] valgrind passed"
