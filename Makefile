CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wno-cast-function-type -Wno-format-truncation -Wno-unused-function -Wno-misleading-indentation -Iinclude
CORE = src/suiye.c src/suiye_crash.c src/suiye_platform.c src/sye_lexer.c src/sye_ast.c src/sye_value.c src/sye_scope.c src/sye_runtime.c
EMBED_CORE = src/suiye_embed.c src/suiye_platform.c src/sye_lexer.c src/sye_ast.c src/sye_value.c src/sye_scope.c src/sye_runtime.c
ECO = GUI Icon_icos Math String FileSystem Json Xml Thread Process DateTime Regex Image Csv Yaml Console Terminal Memory SystemInfo Win32 Clipboard Chart DataFrame SerialPort
NATIVE = Network Http Sqlite Crypto Audio Video Archive WebView

.PHONY: all clean test release eco native

all: bin suiye.exe SuiYeEmbed.dll Pack_it_up.dll Reverse.dll SSLCertificate.dll eco native

bin:
	if not exist bin mkdir bin

suiye.exe: $(CORE) bin
	$(CC) $(CFLAGS) $(CORE) -o bin\$@ -lm

Pack_it_up.dll: modules/pack_it_up.c
	$(CC) $(CFLAGS) -shared $< -o bin\$@

SuiYeEmbed.dll: src/suiye_embed.c $(CORE) bin
	$(CC) $(CFLAGS) -DSUIYE_EMBED_BUILD -shared $(EMBED_CORE) -o bin\$@ -lm

Reverse.dll: modules/reverse.c
	$(CC) $(CFLAGS) -shared $< -o bin\$@

SSLCertificate.dll: modules/ssl_certificate.c
	$(CC) $(CFLAGS) -shared $< -o bin\$@

eco:
	@for %%m in ($(ECO)) do $(CC) $(CFLAGS) -shared modules/eco_module.c -o bin\%%m.dll -DMODULE_NAME=%%m || exit /b 1

native:
	@for %%m in ($(NATIVE)) do $(CC) $(CFLAGS) -shared modules/native_industrial.c -o bin\%%m.dll -DMODULE_NAME=%%m -lws2_32 || exit /b 1

test: all
	tests\run_all.bat

release:
	release.bat

clean:
	clean.bat
