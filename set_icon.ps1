param(
    [string]$IconPath
)

if (-not $IconPath) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $IconPath = Join-Path $scriptDir "icon.ico"
}

if (-not (Test-Path $IconPath)) { exit }

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public class WinAPI {
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern IntPtr LoadImage(IntPtr hinst, string lpszName, uint uType, int cxDesired, int cyDesired, uint fuLoad);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int nIndex);

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public const uint IMAGE_ICON = 1;
    public const uint LR_LOADFROMFILE = 0x00000010;
    public const uint WM_SETICON = 0x0080;
    public const int ICON_SMALL = 0;
    public const int ICON_BIG = 1;

    public const int SM_CXICON = 11;
    public const int SM_CYICON = 12;
    public const int SM_CXSMICON = 49;
    public const int SM_CYSMICON = 50;
}
"@

$cxBig = [WinAPI]::GetSystemMetrics([WinAPI]::SM_CXICON)
$cyBig = [WinAPI]::GetSystemMetrics([WinAPI]::SM_CYICON)
if ($cxBig -lt 32) { $cxBig = 32 }
if ($cyBig -lt 32) { $cyBig = 32 }

$cxSmall = [WinAPI]::GetSystemMetrics([WinAPI]::SM_CXSMICON)
$cySmall = [WinAPI]::GetSystemMetrics([WinAPI]::SM_CYSMICON)
if ($cxSmall -lt 16) { $cxSmall = 16 }
if ($cySmall -lt 16) { $cySmall = 16 }

# Load native-resolution unscaled icons directly from the multi-res ICO file for sharp rendering
$hIconBig = [WinAPI]::LoadImage([IntPtr]::Zero, $IconPath, [WinAPI]::IMAGE_ICON, $cxBig, $cyBig, [WinAPI]::LR_LOADFROMFILE)
$hIconSmall = [WinAPI]::LoadImage([IntPtr]::Zero, $IconPath, [WinAPI]::IMAGE_ICON, $cxSmall, $cySmall, [WinAPI]::LR_LOADFROMFILE)

if ($hIconBig -eq [IntPtr]::Zero -or $hIconSmall -eq [IntPtr]::Zero) { exit }

$script:applied = $false

for ($attempt = 0; $attempt -lt 30; $attempt++) {
    Start-Sleep -Milliseconds 250

    [WinAPI]::EnumWindows({
        param($hWnd, $lParam)
        if ([WinAPI]::IsWindowVisible($hWnd)) {
            $sb = New-Object System.Text.StringBuilder 256
            [WinAPI]::GetWindowText($hWnd, $sb, 256) | Out-Null
            $title = $sb.ToString()
            if ($title -like "*HP71B*" -or $title -like "*Santa Barbara*") {
                [WinAPI]::SendMessage($hWnd, [WinAPI]::WM_SETICON, [IntPtr]1, $hIconBig)
                [WinAPI]::SendMessage($hWnd, [WinAPI]::WM_SETICON, [IntPtr]0, $hIconSmall)
                $script:applied = $true
                return $false
            }
        }
        return $true
    }, [IntPtr]::Zero)

    if ($script:applied) { break }
}
