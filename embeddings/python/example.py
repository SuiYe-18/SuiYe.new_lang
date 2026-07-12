from ctypes import cdll, c_void_p, c_char_p, c_int

lib = cdll.LoadLibrary("../../bin/SuiYeEmbed.dll")
lib.suiye_embed_new.restype = c_void_p
lib.suiye_embed_run_source.argtypes = [c_void_p, c_char_p, c_char_p]
lib.suiye_embed_run_source.restype = c_int
lib.suiye_embed_last_error.argtypes = [c_void_p]
lib.suiye_embed_last_error.restype = c_char_p
lib.suiye_embed_free.argtypes = [c_void_p]

vm = lib.suiye_embed_new()
rc = lib.suiye_embed_run_source(vm, b"python-host", b'println "hello from Python"\n')
if rc:
    print(lib.suiye_embed_last_error(vm).decode())
lib.suiye_embed_free(vm)
