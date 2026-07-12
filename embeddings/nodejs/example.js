const ffi = require("ffi-napi");

const lib = ffi.Library("../../bin/SuiYeEmbed.dll", {
  suiye_embed_new: ["pointer", []],
  suiye_embed_run_source: ["int", ["pointer", "string", "string"]],
  suiye_embed_last_error: ["string", ["pointer"]],
  suiye_embed_free: ["void", ["pointer"]]
});

const vm = lib.suiye_embed_new();
const rc = lib.suiye_embed_run_source(vm, "node-host", 'println "hello from Node.js"\n');
if (rc) console.error(lib.suiye_embed_last_error(vm));
lib.suiye_embed_free(vm);
