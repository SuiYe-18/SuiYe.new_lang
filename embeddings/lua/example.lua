local ffi = require("ffi")
ffi.cdef[[
typedef void SuiYeEmbed;
SuiYeEmbed *suiye_embed_new(void);
int suiye_embed_run_source(SuiYeEmbed *vm, const char *name, const char *source);
void suiye_embed_free(SuiYeEmbed *vm);
]]
local lib = ffi.load("../../bin/SuiYeEmbed.dll")
local vm = lib.suiye_embed_new()
lib.suiye_embed_run_source(vm, "lua-host", 'println "hello from Lua host"\n')
lib.suiye_embed_free(vm)
