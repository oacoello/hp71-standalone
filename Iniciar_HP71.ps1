# ===== HP71 Ballistic Launcher (PowerShell) =====
# Runs hidden - compatible con autenticador sb-acceso

param(
    [string]$sessionToken = "",
    [string]$deviceId = "",
    [string]$applicationCode = "calc"
)

$appDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# ===== READ CLI ARGS (from sb-acceso) =====
for ($i = 0; $i -lt $args.Count; $i++) {
    $arg = $args[$i]
    $nextArg = if ($i + 1 -lt $args.Count) { $args[$i + 1] } else { "" }

    if ($arg -match '^--(sessiontoken|session-token|session_token)=(.+)$') { $sessionToken = $Matches[2] }
    elseif ($arg -match '^--token=(.+)$') { $sessionToken = $Matches[1] }
    elseif ($arg -match '^--(sessiontoken|session-token|session_token|token)$' -and $nextArg) { $sessionToken = $nextArg }
    elseif ($arg -match '^--(deviceid|device-id|device_id)=(.+)$') { $deviceId = $Matches[2] }
    elseif ($arg -match '^--(deviceid|device-id|device_id)$' -and $nextArg) { $deviceId = $nextArg }
    elseif ($arg -match '^--(applicationcode|application-code|application_code)=(.+)$') { $applicationCode = $Matches[2] }
    elseif ($arg -match '^--(applicationcode|application-code|application_code)$' -and $nextArg) { $applicationCode = $nextArg }
}

# ===== SPLASH SCREEN =====
$splashExe = Join-Path $appDir "splash.exe"
$splashPs  = Join-Path $appDir "splash.ps1"
$splashImg = Join-Path $appDir "Splash_screen_hp71b.png"

if (Test-Path $splashExe) {
    Start-Process -FilePath $splashExe -WindowStyle Hidden
} elseif ((Test-Path $splashPs) -and (Test-Path $splashImg)) {
    Start-Process -FilePath "powershell.exe" -ArgumentList "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$splashPs`" -ImagePath `"$splashImg`"" -WindowStyle Hidden
}

# ===== KILL OLD EMULATOR =====
Get-Process -Name "hp71_emulator" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 200

# ===== LAUNCH EMULATOR (hidden) =====
$exePath = Join-Path $appDir "hp71_emulator.exe"
if (Test-Path $exePath) {
    Start-Process -FilePath $exePath -WorkingDirectory $appDir -WindowStyle Hidden
} else {
    [System.Windows.Forms.MessageBox]::Show("No se encontro hp71_emulator.exe en: `n$appDir", "Error HP71B", 0, 16)
    exit 1
}
Start-Sleep -Milliseconds 600

# ===== BUILD URL =====
$appUrl = "http://127.0.0.1:8071"
$queryParams = @()
if ($sessionToken)     { $queryParams += "sessionToken=$sessionToken" }
if ($deviceId)         { $queryParams += "deviceId=$deviceId" }
if ($applicationCode)  { $queryParams += "applicationCode=$applicationCode" }

if ($queryParams.Count -gt 0) {
    $appUrl += "?" + ($queryParams -join "&")
}

# ===== OPEN BROWSER (EDGE / CHROME APP MODE) =====
$iconFile = Join-Path $appDir "icon.ico"
$edgePaths = @(
    "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    "${env:ProgramFiles}\Microsoft\Edge\Application\msedge.exe"
)
$chromePaths = @(
    "${env:ProgramFiles}\Google\Chrome\Application\chrome.exe",
    "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe"
)

$browserPath = ""
foreach ($p in $edgePaths) { if (Test-Path $p) { $browserPath = $p; break } }
if (-not $browserPath) {
    foreach ($p in $chromePaths) { if (Test-Path $p) { $browserPath = $p; break } }
}

if ($browserPath) {
    Start-Process -FilePath $browserPath -ArgumentList "--app=`"$appUrl`""
} else {
    Start-Process $appUrl
}

# ===== SET TASKBAR ICON =====
$setIconPs = Join-Path $appDir "set_icon.ps1"
if ((Test-Path $setIconPs) -and (Test-Path $iconFile)) {
    Start-Process -FilePath "powershell.exe" -ArgumentList "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$setIconPs`" -IconPath `"$iconFile`"" -WindowStyle Hidden
}

# ===== KEEP ALIVE (check every 2.5s) =====
while ($true) {
    Start-Sleep -Milliseconds 2500
    $proc = Get-Process -Name "hp71_emulator" -ErrorAction SilentlyContinue
    if (-not $proc) { break }
}
