package main

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../bin -lSuiYeEmbed
#include "suiye_embed.h"
*/
import "C"

func main() {
    vm := C.suiye_embed_new()
    C.suiye_embed_run_source(vm, C.CString("go-host"), C.CString("println \"hello from Go\"\n"))
    C.suiye_embed_free(vm)
}
