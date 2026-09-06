param([string]$IconPath)

Add-Type -AssemblyName System.Drawing

$icon = New-Object System.Drawing.Icon($IconPath)
$hIcon = $icon.Handle

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinAPI {
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    public const uint WM_SETICON = 0x0080;
}
"@

Start-Sleep -Seconds 1
$procs = Get-Process msedge -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero }
if ($procs) {
    $hWnd = $procs[0].MainWindowHandle
    [WinAPI]::SendMessage($hWnd, [WinAPI]::WM_SETICON, [IntPtr]1, $hIcon)
    [WinAPI]::SendMessage($hWnd, [WinAPI]::WM_SETICON, [IntPtr]0, $hIcon)
}
