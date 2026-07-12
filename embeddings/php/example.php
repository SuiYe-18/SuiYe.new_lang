<?php
$ffi = FFI::cdef("
typedef void SuiYeEmbed;
SuiYeEmbed *suiye_embed_new(void);
int suiye_embed_run_source(SuiYeEmbed *vm, const char *name, const char *source);
void suiye_embed_free(SuiYeEmbed *vm);
", "../../bin/SuiYeEmbed.dll");
$vm = $ffi->suiye_embed_new();
$ffi->suiye_embed_run_source($vm, "php-host", "println \"hello from PHP\"\n");
$ffi->suiye_embed_free($vm);
?>
