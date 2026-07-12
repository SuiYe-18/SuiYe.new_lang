$sig = @"
using System;
using System.Runtime.InteropServices;
public static class SuiYeEmbed {
  [DllImport("../../bin/SuiYeEmbed.dll", CallingConvention=CallingConvention.Cdecl)] public static extern IntPtr suiye_embed_new();
  [DllImport("../../bin/SuiYeEmbed.dll", CallingConvention=CallingConvention.Cdecl)] public static extern int suiye_embed_run_source(IntPtr vm, string name, string source);
  [DllImport("../../bin/SuiYeEmbed.dll", CallingConvention=CallingConvention.Cdecl)] public static extern void suiye_embed_free(IntPtr vm);
}
"@
Add-Type $sig
$vm = [SuiYeEmbed]::suiye_embed_new()
[SuiYeEmbed]::suiye_embed_run_source($vm, "powershell-host", "println `"hello from PowerShell`"`n")
[SuiYeEmbed]::suiye_embed_free($vm)
