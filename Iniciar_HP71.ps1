# ===== HP71 Ballistic Launcher (PowerShell) =====
# Runs hidden - no wscript.exe in taskbar

param(
    [string]$sessionToken = "",
    [string]$deviceId = "",
    [string]$applicationCode = "calc"
)

$appDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# ===== READ CLI ARGS (from sb-acceso) =====
foreach ($arg in $args) {
    if ($arg -match '^--sessiontoken=(.+)$') { $sessionToken = $Matches[1] }
    elseif ($arg -match '^--session-token=(.+)$') { $sessionToken = $Matches[1] }
    elseif ($arg -match '^--deviceid=(.+)$') { $deviceId = $Matches[1] }
    elseif ($arg -match '^--device-id=(.+)$') { $deviceId = $Matches[1] }
    elseif ($arg -match '^--applicationcode=(.+)$') { $applicationCode = $Matches[1] }
    elseif ($arg -match '^--application-code=(.+)$') { $applicationCode = $Matches[1] }
}

# ===== SPLASH SCREEN =====
$splashPs = Join-Path $appDir "splash.ps1"
$splashGif = Join-Path $appDir "Splash.gif"
if ((Test-Path $splashPs) -and (Test-Path $splashGif)) {
    Start-Process -FilePath "powershell.exe" -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$splashPs`" -GifPath `"$splashGif`"" -WindowStyle Hidden -Wait
}

# ===== KILL OLD EMULATOR =====
Get-Process -Name "hp71_emulator" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

# ===== LAUNCH EMULATOR (hidden) =====
$exePath = Join-Path $appDir "hp71_emulator.exe"
if (Test-Path $exePath) {
    Start-Process -FilePath $exePath -WindowStyle Hidden
}
Start-Sleep -Seconds 1

# ===== BUILD URL =====
$appUrl = "http://127.0.0.1:8080"
if ($sessionToken) {
    $appUrl += "?sessionToken=$sessionToken"
    if ($deviceId) { $appUrl += "&deviceId=$deviceId" }
    if ($applicationCode) { $appUrl += "&applicationCode=$applicationCode" }
}

# ===== OPEN BROWSER (via shortcut for taskbar icon) =====
$iconFile = Join-Path $appDir "icon.ico"
$edgePath = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe"
$chromePath = "${env:ProgramFiles}\Google\Chrome\Application\chrome.exe"
$browserPath = ""

if (Test-Path $edgePath) { $browserPath = $edgePath }
elseif (Test-Path $chromePath) { $browserPath = $chromePath }

if ($browserPath) {
    # Create temporary shortcut with icon so taskbar shows it
    $tempShortcut = Join-Path $env:TEMP "HP71_Calc.lnk"
    $shell = New-Object -ComObject WScript.Shell
    $lnk = $shell.CreateShortcut($tempShortcut)
    $lnk.TargetPath = $browserPath
    $lnk.Arguments = "--app=`"$appUrl`""
    $lnk.IconLocation = "$iconFile,0"
    $lnk.Save()

    # Launch from shortcut (taskbar inherits shortcut icon)
    Start-Process -FilePath $tempShortcut
    Start-Sleep -Seconds 2

    # Remove temp shortcut
    Remove-Item $tempShortcut -Force -ErrorAction SilentlyContinue
} else {
    Start-Process $appUrl
}

# ===== KEEP ALIVE (check every 3s) =====
while ($true) {
    Start-Sleep -Seconds 3
    $proc = Get-Process -Name "hp71_emulator" -ErrorAction SilentlyContinue
    if (-not $proc) { break }
}
