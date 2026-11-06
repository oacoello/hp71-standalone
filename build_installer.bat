@echo off
setlocal enabledelayedexpansion

echo =====================================================================
echo   Compilacion de Calculadora HP71B e Instalador Inno Setup
echo =====================================================================
echo.

cd /d "%~dp0"

:: 1. Compilar C++ si CMake y MSVC estan disponibles
echo [1/4] Verificando compilacion de C++ (hp71_emulator.exe)...
if not exist "hp71_emulator.exe" (
    where cmake >nul 2>nul
    if !errorlevel! equ 0 (
        echo [*] Compilando hp71_emulator con CMake...
        if not exist build mkdir build
        cd build
        cmake .. -DCMAKE_BUILD_TYPE=Release
        cmake --build . --config Release
        cd ..
        if exist "build\hp71_server\Release\hp71_emulator.exe" (
            copy /Y "build\hp71_server\Release\hp71_emulator.exe" ".\hp71_emulator.exe"
            echo [OK] hp71_emulator.exe generado exitosamente.
        )
    ) else (
        echo [!] CMake no disponible en PATH. Si ya tienes hp71_emulator.exe copialo a esta carpeta.
    )
) else (
    echo [OK] hp71_emulator.exe ya esta presente en la raiz.
)

:: 2. Compilar splash.cs si csc.exe existe
echo [2/4] Verificando splash.exe...
if not exist "splash.exe" (
    set CSC="C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
    if exist !CSC! (
        if exist "splash.cs" (
            echo [*] Compilando splash.exe con csc...
            !CSC! /nologo /target:winexe /out:splash.exe /win32icon:icon.ico splash.cs >nul 2>nul
            if exist "splash.exe" echo [OK] splash.exe compilado.
        )
    )
)

:: 3. Asegurar tables.csv en raiz
echo [3/4] Verificando tables.csv...
if not exist "tables.csv" (
    if exist "tables\tables.csv" copy "tables\tables.csv" "tables.csv" >nul
)
if exist "tables.csv" (
    echo [OK] tables.csv listo.
) else (
    echo [!] Advertencia: tables.csv no encontrado en la raiz ni en tables\.
)

:: 4. Compilar instalador con Inno Setup
echo [4/4] Buscando Inno Setup Compiler (ISCC.exe)...
set ISCC=""

if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set ISCC="%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set ISCC="%ProgramFiles%\Inno Setup 6\ISCC.exe"
if exist "%ProgramFiles(x86)%\Inno Setup 5\ISCC.exe" set ISCC="%ProgramFiles(x86)%\Inno Setup 5\ISCC.exe"
if exist "%ProgramFiles%\Inno Setup 5\ISCC.exe" set ISCC="%ProgramFiles%\Inno Setup 5\ISCC.exe"
if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set ISCC="%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"

if !ISCC!=="" (
    where iscc >nul 2>nul
    if !errorlevel! equ 0 set ISCC="iscc"
)

if not !ISCC!=="" (
    echo [*] Compilando installer.iss con Inno Setup...
    if not exist dist mkdir dist
    !ISCC! "installer.iss"
    if !errorlevel! equ 0 (
        echo.
        echo =====================================================================
        echo   [EXITO] Instalador compilado correctamente:
        echo   dist\Calculadora_HP71B_Installer.exe
        echo =====================================================================
    ) else (
        echo [ERROR] Error durante la compilacion de Inno Setup.
    )
) else (
    echo.
    echo [!] No se encontro ISCC.exe de Inno Setup en el sistema.
    echo     Puedes compilarlo manualmente abriendo 'installer.iss' en Inno Setup,
    echo     o descargando Inno Setup desde: https://jrsoftware.org/isdl.php
)

echo.
pause
