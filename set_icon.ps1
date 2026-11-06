param(
    [string]$IconPath
)

if (-not $IconPath) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $IconPath = Join-Path $scriptDir "icon.ico"
}

if (-not (Test-Path $IconPath)) { exit }

Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public class WinAPI {
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll", EntryPoint = "SetClassLongPtr")]
    public static extern IntPtr SetClassLongPtr64(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

    [DllImport("user32.dll", EntryPoint = "SetClassLong")]
    public static extern uint SetClassLong32(IntPtr hWnd, int nIndex, uint dwNewLong);

    public static void SetClassIcon(IntPtr hWnd, IntPtr hIconBig, IntPtr hIconSmall) {
        try {
            if (IntPtr.Size == 8) {
                SetClassLongPtr64(hWnd, -14, hIconBig);   // GCLP_HICON
                SetClassLongPtr64(hWnd, -34, hIconSmall); // GCLP_HICONSM
            } else {
                SetClassLong32(hWnd, -14, (uint)hIconBig.ToInt32());
                SetClassLong32(hWnd, -34, (uint)hIconSmall.ToInt32());
            }
        } catch {}
    }

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public const uint WM_SETICON = 0x0080;
    public const int ICON_SMALL = 0;
    public const int ICON_BIG = 1;
}
"@

# Cargar icono via System.Drawing (soporta ICO nativo, PNG, y multi-resolución)
$hIconBig = [IntPtr]::Zero
$hIconSmall = [IntPtr]::Zero

try {
    $rawBmp = [System.Drawing.Bitmap]::FromFile($IconPath)
    $hIconBig = $rawBmp.GetHicon()
    $hIconSmall = $hIconBig
} catch {
    try {
        $iconObj = New-Object System.Drawing.Icon($IconPath)
        $hIconBig = $iconObj.Handle
        $hIconSmall = $iconObj.Handle
    } catch {
        exit
    }
}

if ($hIconBig -eq [IntPtr]::Zero) { exit }

$script:applied = $false

for ($attempt = 0; $attempt -lt 40; $attempt++) {
    Start-Sleep -Milliseconds 250

    [WinAPI]::EnumWindows({
        param($hWnd, $lParam)
        if ([WinAPI]::IsWindowVisible($hWnd)) {
            $sb = New-Object System.Text.StringBuilder 256
            [WinAPI]::GetWindowText($hWnd, $sb, 256) | Out-Null
            $title = $sb.ToString()

            $pid = 0
            [WinAPI]::GetWindowThreadProcessId($hWnd, [ref]$pid) | Out-Null
            $procName = ""
            if ($pid -gt 0) {
                try { $procName = (Get-Process -Id $pid -ErrorAction SilentlyContinue).ProcessName } catch {}
            }

            # Si es ventana de Edge o Chrome y el titulo coincide con Santa Barbara o HP71
            $isTargetTitle = ($title -like "*SANTA BARBARA*" -or $title -like "*Santa Barbara*" -or $title -like "*HP71*" -or $title -like "*HP-71*")
            $isBrowser = ($procName -like "*msedge*" -or $procName -like "*chrome*")

            if ($isTargetTitle -or ($isBrowser -and $title -ne "" -and $title -notlike "*tab*" -and $title -notlike "*nueva pestaña*")) {
                if ($isTargetTitle) {
                    [WinAPI]::SendMessage($hWnd, [WinAPI]::WM_SETICON, [IntPtr]1, $hIconBig)   | Out-Null
                    [WinAPI]::SendMessage($hWnd, [WinAPI]::WM_SETICON, [IntPtr]0, $hIconSmall) | Out-Null
                    [WinAPI]::SetClassIcon($hWnd, $hIconBig, $hIconSmall)
                    $script:applied = $true
                }
            }
        }
        return $true
    }, [IntPtr]::Zero)

    if ($script:applied -and $attempt -ge 10) { break }
}
