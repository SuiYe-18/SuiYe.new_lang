# SuiYe tests

Run all regression checks from the project root:

```bat
tests\run_all.bat
```

The test runner verifies:

1. `bin\suiye.exe --version`
2. `bin\suiye.exe --syntax`
3. `bin\suiye.exe --run examples\all_syntax.sye`
4. Legacy direct run: `bin\suiye.exe examples\round5_regression.sye`
5. Pack command: `bin\suiye.exe --pack examples\all_syntax.sye all_syntax_test.exe`
6. Packed executable execution: `build\dist\all_syntax_test.exe`
7. Reverse command: `bin\suiye.exe --reverse build\dist\all_syntax_test.exe build\reverse\restored_all_syntax_test.sye`
