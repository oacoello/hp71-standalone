# =====================================================================
# Compilacion automatica de Calculadora HP71B e Instalador Inno Setup
# =====================================================================

$ErrorActionPreference = "Continue"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectRoot

Write-Host "=====================================================================" -ForegroundColor Cyan
Write-Host "  Compilando Calculadora HP71B e Instalador" -ForegroundColor Cyan
Write-Host "=====================================================================" -ForegroundColor Cyan
Write-Host ""

# 1. Compilar C++ si CMake está disponible
Write-Host "[1/4] Verificando compilacion de hp71_emulator.exe..." -ForegroundColor Yellow
$exePath = Join-Path $projectRoot "hp71_emulator.exe"
if (-not (Test-Path $exePath)) {
    $cmakeCmd = Get-Command "cmake" -ErrorAction SilentlyContinue
    if ($cmakeCmd) {
        Write-Host "  -> Ejecutando CMake..." -ForegroundColor Gray
        $buildDir = Join-Path $projectRoot "build"
        if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }
        
        cmake -B "$buildDir" -S "$projectRoot" -DCMAKE_BUILD_TYPE=Release
        cmake --build "$buildDir" --config Release
        
        $builtExe = Join-Path $buildDir "hp71_server\Release\hp71_emulator.exe"
        if (Test-Path $builtExe) {
            Copy-Item -Path $builtExe -Destination $exePath -Force
            Write-Host "  [OK] hp71_emulator.exe compilado y copiado." -ForegroundColor Green
        }
    } else {
        Write-Host "  [!] CMake no detectado. Si ya dispones del ejecutable, copialo como hp71_emulator.exe." -ForegroundColor DarkYellow
    }
} else {
    Write-Host "  [OK] hp71_emulator.exe ya presente en raiz." -ForegroundColor Green
}

# 2. Compilar splash.cs si es necesario
Write-Host "[2/4] Verificando splash.exe..." -ForegroundColor Yellow
$splashExe = Join-Path $projectRoot "splash.exe"
$splashCs = Join-Path $projectRoot "splash.cs"
if ((-not (Test-Path $splashExe)) -and (Test-Path $splashCs)) {
    $csc = "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
    if (Test-Path $csc) {
        Write-Host "  -> Compilando splash.cs..." -ForegroundColor Gray
        Start-Process -FilePath $csc -ArgumentList "/nologo /target:winexe /out:`"$splashExe`" /win32icon:`"icon.ico`" `"$splashCs`"" -NoNewWindow -Wait
        if (Test-Path $splashExe) {
            Write-Host "  [OK] splash.exe generado." -ForegroundColor Green
        }
    }
}

# 3. Asegurar tables.csv en raiz
Write-Host "[3/4] Verificando tables.csv..." -ForegroundColor Yellow
$tablesCsv = Join-Path $projectRoot "tables.csv"
if (-not (Test-Path $tablesCsv)) {
    $srcTables = Join-Path $projectRoot "tables\tables.csv"
    if (Test-Path $srcTables) {
        Copy-Item -Path $srcTables -Destination $tablesCsv -Force
    }
}
if (Test-Path $tablesCsv) {
    Write-Host "  [OK] tables.csv listo." -ForegroundColor Green
}

# 4. Compilar con Inno Setup
Write-Host "[4/4] Buscando Inno Setup (ISCC.exe)..." -ForegroundColor Yellow
$isccCandidates = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 5\ISCC.exe",
    "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
)

$isccPath = ""
foreach ($cand in $isccCandidates) {
    if (Test-Path $cand) { $isccPath = $cand; break }
}
if (-not $isccPath) {
    $cmd = Get-Command "iscc" -ErrorAction SilentlyContinue
    if ($cmd) { $isccPath = $cmd.Source }
}

if ($isccPath) {
    Write-Host "  -> Compilando installer.iss con Inno Setup..." -ForegroundColor Cyan
    $distDir = Join-Path $projectRoot "dist"
    if (-not (Test-Path $distDir)) { New-Item -ItemType Directory -Path $distDir | Out-Null }
    
    $proc = Start-Process -FilePath $isccPath -ArgumentList "`"$(Join-Path $projectRoot 'installer.iss')`"" -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -eq 0) {
        Write-Host ""
        Write-Host "=====================================================================" -ForegroundColor Green
        Write-Host "  [EXITO] Instalador creado en: dist\Calculadora_HP71B_Installer.exe" -ForegroundColor Green
        Write-Host "=====================================================================" -ForegroundColor Green
    } else {
        Write-Host "  [ERROR] Fallo la compilacion del instalador (codigo $($proc.ExitCode))." -ForegroundColor Red
    }
} else {
    Write-Host "  [!] Inno Setup no encontrado en las rutas estandar." -ForegroundColor DarkYellow
    Write-Host "      Puedes compilar abriendo 'installer.iss' en Inno Setup Compiler" -ForegroundColor Gray
    Write-Host "      o descargando Inno Setup desde https://jrsoftware.org/isdl.php" -ForegroundColor Gray
}
