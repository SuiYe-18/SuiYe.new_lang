using System;
using System.Runtime.InteropServices;

class Program {
  [DllImport("../../bin/SuiYeEmbed.dll", CallingConvention = CallingConvention.Cdecl)] static extern IntPtr suiye_embed_new();
  [DllImport("../../bin/SuiYeEmbed.dll", CallingConvention = CallingConvention.Cdecl)] static extern int suiye_embed_run_source(IntPtr vm, string name, string source);
  [DllImport("../../bin/SuiYeEmbed.dll", CallingConvention = CallingConvention.Cdecl)] static extern IntPtr suiye_embed_last_error(IntPtr vm);
  [DllImport("../../bin/SuiYeEmbed.dll", CallingConvention = CallingConvention.Cdecl)] static extern void suiye_embed_free(IntPtr vm);

  static int Main() {
    var vm = suiye_embed_new();
    int rc = suiye_embed_run_source(vm, "csharp-host", "println \"hello from C#\"\n");
    if (rc != 0) Console.Error.WriteLine(Marshal.PtrToStringAnsi(suiye_embed_last_error(vm)));
    suiye_embed_free(vm);
    return rc;
  }
}
