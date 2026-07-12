# Embedding SuiYe from other languages

The stable C ABI is defined in `include/suiye_api.h`. Native callers can load `suiye.exe`-adjacent DLL modules and call their exported `suiye_module_init` and `suiye_module_cli` functions.

For broad language access, use each language's native FFI/dynamic library loader. This works with C, C++, Rust, Go, Zig, D, C#, F#, VB.NET, Java JNI/JNA, Kotlin/JVM, Scala, Python ctypes/cffi, Ruby Fiddle, PHP FFI, Node.js ffi-napi, Deno FFI, LuaJIT FFI, Perl FFI::Platypus, R, Julia, Swift, Objective-C, Delphi, FreePascal, Nim, Crystal, Haskell, OCaml, Erlang NIF, Elixir NIF, Racket, Tcl, PowerShell, AutoHotkey, AutoIt, LabVIEW, MATLAB, Mathematica, Groovy, Clojure, COBOL FFI, Fortran ISO_C_BINDING, Ada, and Scheme implementations with C FFI.
