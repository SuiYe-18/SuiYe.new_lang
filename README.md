# SuiYe

SuiYe is a C-built interpreted and embeddable language prototype with the `.sye` suffix.

Creator: `SuiYe_TR`

## Build

Run:

```bat
build.bat release
build.bat debug
```

The build uses GCC and writes `suiye.exe`, `Pack_it_up.dll`, `Reverse.dll`, `SSLCertificate.dll`, and ecosystem DLLs to `bin`.

Current folder layout:

```text
bin\       runtime executable and DLLs
build\     generated dist/log/reverse/test/release outputs
docs\      developer and module documentation
embeddings\ 45 host-language embedding examples
examples\  example .sye scripts
include\   public C headers
modules\   DLL module source
src\       interpreter/runtime source
tests\     regression tests
```

Engineering helpers:

```bat
clean.bat
release.bat
tests\run_all.bat
quality_gate.bat
```

Additional build files are available:

```text
CMakeLists.txt
Makefile
.github\workflows\windows.yml
```

## Run

```bat
bin\suiye.exe examples\hello.sye
bin\suiye.exe --run examples\hello.sye
```

## Pack

```bat
bin\suiye.exe Pack_it_up.dll -o examples\hello.sye "hello.exe"
bin\suiye.exe --pack examples\hello.sye "hello.exe"
bin\suiye.exe --pack-ast examples\hello.sye "hello.exe"
```

Output:

```text
build\dist\hello.exe
build\log\hello.suiye-pack.cfg
build\log\hello.suiye-pack.json
```

Icon metadata can be recorded with:

```bat
bin\suiye.exe Pack_it_up.dll Icon_icos.dll -o examples\hello.sye -i "icon.ico" "hello.exe"
bin\suiye.exe --pack-icon examples\hello.sye "icon.ico" "hello.exe"
```

## Reverse

```bat
bin\suiye.exe Reverse.dll build\dist\hello.exe -o build\reverse\restored_hello.sye
bin\suiye.exe --reverse build\dist\hello.exe build\reverse\restored_hello.sye
```

The restored source starts with:

```text
GPYT|'/'| Reverse Parsing
```

Reverse validates the Pack V3 feature code, source hash, and byte count before writing output. It also writes a `*.reverse-report.json` report next to the restored source.

The interpreter skips this reverse marker when running or repacking.

## Documentation

```text
docs\DEVELOPER_GUIDE.md
docs\PACK_REVERSE.md
docs\SUIDB.md
docs\MODULE_COMMANDS.md
docs\ERROR_CODES.md
docs\QUICK_START.md
docs\EMBEDDING.md
docs\INDUSTRIAL_GRADE.md
docs\CROSS_PLATFORM.md
docs\ABI_VERSIONING.md
docs\TOOLS.md
embeddings\README.md
tools\suiye_lsp.py
tools\suiye_pkg.py
CHANGELOG.md
```

## Certificate

```bat
suiye SSLCertificate.dll -o CN key\pem "365"
```

This generates `key\pem.key` and `key\pem.pem` with SuiYe certificate metadata and an expiry interval.

## Command line

Formal command line aliases:

```bat
suiye --help
suiye --version
suiye --syntax
suiye --check examples\all_syntax.sye
suiye --run examples\hello.sye
suiye --pack examples\hello.sye hello.exe
suiye --pack-ast examples\hello.sye hello.exe
suiye --pack-icon examples\hello.sye icon.ico hello.exe
suiye --reverse dist\hello.exe restored.sye
```

Legacy commands are still supported:

```bat
suiye examples\hello.sye
suiye Pack_it_up.dll -o examples\hello.sye "hello.exe"
suiye Reverse.dll dist\hello.exe -o restored.sye
```

## Tests

Run the regression suite:

```bat
tests\run_all.bat
```

## Interpreter core part 1

The first third of the interpreter-core rewrite is complete as an independent, tested foundation:

1. Strict lexer: implemented in `include/sye_lexer.h` and `src/sye_lexer.c`.
2. AST parser: implemented in `include/sye_ast.h` and `src/sye_ast.c`.
3. Real value model foundation: implemented in `include/sye_value.h` and `src/sye_value.c`.
4. Function local scope, module scope, and closure data structures: implemented in `include/sye_scope.h` and `src/sye_scope.c`.
5. Syntax checking command: `suiye --check xxx.sye`.

Run checks:

```bat
suiye --check tests\core_part1_check.sye
suiye --check examples\all_syntax.sye
```

The old line interpreter remains active for legacy execution. The new AST runtime is available through `suiye --ast-run xxx.sye`.

## Interpreter core part 2

The second third of the interpreter-core rewrite is complete as a runnable AST runtime foundation:

1. AST Runtime: implemented in `include/sye_runtime.h` and `src/sye_runtime.c`.
2. `suiye --ast-run xxx.sye` executes scripts through AST nodes instead of the old line interpreter.
3. Function calls now run in local scopes.
4. Function return values are transferred to the caller through `_`.
5. Runtime values use `SyeValue`, including `bool`, `null`, `number`, `string`, `array`, `object`, and `function`.
6. Arrays and maps execute as real runtime objects in the AST runtime.
7. `try/catch`, `for`, `repeat`, `while`, `if/else`, `return`, `break`, and `continue` are implemented in the AST runtime subset.

Run the AST runtime test:

```bat
suiye --ast-run tests\core_part2_ast_runtime.sye
```

## Interpreter core part 3

The third third of the interpreter-core rewrite is complete for the AST runtime diagnostics layer:

1. AST runtime errors now print richer diagnostics.
2. Function calls maintain a call stack for runtime failures.
3. `include` execution maintains an include stack.
4. Module loading maintains a module stack for diagnostics.
5. `include` now executes files through the AST runtime.
6. Common standard-library commands were migrated to the AST runtime subset: `read`, `write`, `save`, `append`, `exists`, `delete`, `pwd`, `cd`, `mkdir`, `rmdir`, `now`, `random`, `upper`, `lower`, `modules`, and `commands`.
7. Module loading works in the AST runtime with ABI, permission, and dependency metadata checks.
8. Direct `module.command` dispatch works in the AST runtime.

Run the successful part 3 test:

```bat
suiye --ast-run tests\core_part3_ast_runtime.sye
```

Run the expected-failure diagnostics tests:

```bat
suiye --ast-run tests\core_part3_error.sye
suiye --ast-run tests\core_part3_active_include_error.sye
suiye --ast-run tests\core_part3_module_error.sye
```

The interpreter core rewrite is now split into a checked AST frontend, a runnable AST runtime, and a diagnostics layer. The old line interpreter remains available as the default `--run` path for compatibility, while `--ast-run` is the new runtime path.

## DLL ecosystem completion

The DLL ecosystem has been upgraded:

1. AST Runtime now dispatches real `module.command` calls, such as `GUI.echo`, `Math.add`, `Crypto.sha1`, `Http.url_parse`, `Sqlite.sqlite_open`, and `Network.ping`.
2. DLLs export ABI metadata through `suiye_module_abi_version`.
3. DLLs export permission metadata through `suiye_module_permissions`.
4. DLLs export dependency metadata through `suiye_module_dependencies`.
5. The ecosystem DLL template now exposes 38 commands per module instead of 20.
6. GUI, Icon, HTTP, Network, Sqlite, and Crypto now have specialized diagnostic commands in addition to common commands.
7. All ecosystem DLLs now expose an industrial baseline of real system-backed commands:
   `sha256`, `http_get`, `clipboard_set`, `clipboard_get`, `sysinfo`, `process_run`, `file_exists`, `csv_info`, `json_get`, `match`, and `industrial_status`.
8. `sha256` uses Windows `bcrypt.dll` dynamically.
9. `http_get` uses Windows `wininet.dll` dynamically.
10. Missing engines are reported as real errors instead of fake success.

Run the DLL dispatch test:

```bat
suiye --ast-run tests\dll_ast_dispatch.sye
```

Run the industrial baseline test:

```bat
suiye --ast-run tests\dll_industrial_baseline.sye
```

Industrial baseline means every DLL is loadable, ABI-checked, permission/dependency-reported, and provides real callable commands.

## SuiYe-native industrial modules

The modules that previously needed large external engines have been switched to fully self-developed SuiYe-native implementations:

1. `Sqlite.dll` now exposes SuiDB, a SuiYe-native file database engine: `sqlite_open`, `db_insert`, `db_select`, `db_count`, and `db_delete_table`.
2. `Http.dll` and `Network.dll` now expose a SuiYe-native HTTP/1.0 TCP client and URL/network diagnostics: `http_get`, `http_head`, `url_parse`, and `ping`.
3. `Crypto.dll` now exposes self-developed crypto primitives: `sha256` and `crypto_xor`.
4. `Archive.dll` now exposes the SuiYe `.sya` archive format: `archive_pack`, `archive_list`, and `archive_extract_text`.
5. `WebView.dll` now exposes SuiYe-native HTML rendering: `webview` and `webview_component`.
6. `Audio.dll` now exposes WAV parsing, tone generation, and native beep: `wav_info`, `wav_tone`, and `beep`.
7. `Video.dll` now exposes native media file signature and hash analysis: `video_info` and `video_hash`.
8. All these modules report `external_engine=false` through `native_status`.
9. The remaining ecosystem modules now expose dedicated self-researched capabilities, including `icon_make`, `bmp_info`, `chart_bar`, `string_replace`, `math_stats`, `cert_sui`, `dataframe_sum`, `xml_get`, and `yaml_get`.
10. All upgraded modules report `status=self_research_100` and `completion=100` for the current SuiYe-native scope.

Run the SuiYe-native industrial module test:

```bat
suiye --ast-run tests\native_industrial_modules.sye
```

Run the 100 percent self-research completion test:

```bat
suiye --ast-run tests\self_research_100.sye
```

Run the high-depth optimization test:

```bat
suiye --ast-run tests\high_depth_native.sye
```

High-depth additions include SuiDB `db_where`, `db_update`, and `db_export_csv`; HTTP `http_build_request` and `http_status_parse`; Crypto `crypto_random` and `crypto_hmac_sui`; and Video `video_format` and `video_scan_signature`.

## Pack V3

The packer now supports V3 packaging features and a cleaned output layout:

1. `--ast` embeds an AST runtime startup directive, so the packed executable runs through AST Runtime by default.
2. Packed executables are written to `build\dist`.
3. Pack configs and metadata are written to `build\log`.
4. Reverse outputs should be written to `build\reverse`.
5. Runtime DLL dependencies are copied next to the executable in `build\dist`.
6. `--deps` also writes a package-specific dependency snapshot to `build\dist\<name>.deps`.
7. `--add path` copies resource files or directories to `build\dist\<name>.resources`.
8. `--product`, `--version`, and `--company` write pack metadata.
9. Every pack generates a different feature code containing digits, letters, and symbols.
10. `-i icon.ico` attempts real Windows ICO resource injection through `BeginUpdateResource`.
11. Reverse parsing refuses executables without the SuiYe feature code.
12. Packed executables include metadata with FNV-1a integrity hash, byte count, and feature code.
13. Reverse parsing ignores pack metadata and restores only the `.sye` source after feature-code verification.

Example:

```bat
suiye Pack_it_up.dll --ast --deps --add tests\pack_resource.txt --product SuiYeAST --version 2.0.0 --company SuiYeTeam -o tests\core_part3_ast_runtime.sye ast_pack_test.exe
build\dist\ast_pack_test.exe
suiye --reverse build\dist\ast_pack_test.exe build\reverse\restored_ast_pack_test.sye
```

## Syntax catalog

Run:

```bat
suiye --syntax
```

The current build exposes 58 syntax entries, including comments, variables, constants, assignment, output, input, conditions, loops, functions, import/use/load, file I/O, process/system helpers, string helpers, arrays, maps, JSON wrapper syntax, module inspection commands, and module commands.

## Round 5 improvements

This pass added five concrete improvements:

1. Runtime errors now include source file and line information.
2. `} else {` is normalized while loading scripts, so common inline else formatting works.
3. Expressions now use precedence parsing for parentheses, unary `-`, `not`, `* / %`, `+ -`, comparison, `and`, and `or`.
4. Module loading searches the current directory, `modules`, and `dll`.
5. New `modules` and `commands` script commands list loaded DLLs and registered module commands.

Run the regression script:

```bat
suiye examples\round5_regression.sye
```

## Complete syntax pass

This pass completed the syntax catalog that was previously partial:

1. `for` now runs real numeric loops, including `for i 1 3 { ... }` and `for i in 1 3 { ... }`.
2. `try` and `catch` now implement real error capture. Captured errors enter the `catch` block without printing an uncaught runtime error.
3. `array`, `push`, `at`, and `pop` provide basic list storage and access.
4. `map`, `put`, and `get` provide basic key-value storage and access.
5. `json` now escapes strings before writing JSON-style text.
6. DLL loading now searches the current directory, `modules`, `dll`, the executable directory, and the executable parent directory, so packed programs can still find root-level DLLs after `cd`.

Run the complete syntax regression:

```bat
suiye examples\all_syntax.sye
```

Pack and test it:

```bat
suiye Pack_it_up.dll -o examples\all_syntax.sye "all_syntax.exe"
dist\all_syntax.exe
suiye Reverse.dll dist\all_syntax.exe -o restored_all_syntax.sye
```

## DLL ecosystem

The generated ecosystem includes:

`GUI`, `Icon_icos`, `Math`, `String`, `FileSystem`, `Network`, `Http`, `Json`, `Xml`, `Sqlite`, `Crypto`, `Thread`, `Process`, `DateTime`, `Regex`, `Image`, `Audio`, `Video`, `Archive`, `Csv`, `Yaml`, `Console`, `Terminal`, `Memory`, `SystemInfo`, `Win32`, `Clipboard`, `WebView`, `Chart`, `DataFrame`, `SerialPort`.

Each generic ecosystem DLL exports 20 implemented commands:

`info`, `echo`, `set`, `add`, `mul`, `time`, `write`, `read`, `upper`, `lower`, `len`, `random`, `sleep`, `box`, `gui`, `json`, `hash`, `env`, `cli`, `version`.

Command form inside `.sye`:

```sye
import GUI.dll
GUI.info
GUI.gui "window.html"
```

## Scope note

This repository is a working foundation, not a finished industrial language distribution. The build is intentionally pure C and separated into `.c` and `.h` files, but the full request for a 10,000 to 15,000 line industrial runtime, full Qt-equivalent GUI toolkit, real OpenSSL-compatible X.509 generation, and official bindings for 45+ programming languages is larger than a single-session implementation.
