param(
    [string]$ImagePath
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $ImagePath) { 
    $ImagePath = Join-Path $scriptDir "Splash_screen_hp71b.png" 
}
if (-not (Test-Path $ImagePath)) { exit }

try {
    $img = [System.Drawing.Image]::FromFile($ImagePath)
} catch {
    exit
}

$readyFile = Join-Path $env:TEMP "hp71_splash.ready"

$form = New-Object System.Windows.Forms.Form
$form.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen
$form.TopMost = $true
$form.ShowInTaskbar = $false
$form.Size = New-Object System.Drawing.Size(781, 525)
$form.BackColor = [System.Drawing.Color]::Black

$pic = New-Object System.Windows.Forms.PictureBox
$pic.Dock = [System.Windows.Forms.DockStyle]::Fill
$pic.SizeMode = [System.Windows.Forms.PictureBoxSizeMode]::StretchImage
$pic.Image = $img
$form.Controls.Add($pic)

$closeTimer = New-Object System.Windows.Forms.Timer
$closeTimer.Interval = 1200
$closeTimer.Add_Tick({
    $closeTimer.Stop()
    $form.Close()
})

$form.Add_Shown({
    try { [System.IO.File]::WriteAllText($readyFile, "1") } catch {}
    $closeTimer.Start()
    $form.Activate()
})

[System.Windows.Forms.Application]::Run($form)

try { if (Test-Path $readyFile) { Remove-Item $readyFile -Force -ErrorAction SilentlyContinue } } catch {}

$closeTimer.Dispose()
$pic.Dispose()
$form.Dispose()
$img.Dispose()
