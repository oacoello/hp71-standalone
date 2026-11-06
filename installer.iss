; =====================================================================
; Script Inno Setup - Calculadora Balística HP-71B (Santa Bárbara)
; Compatible con Inno Setup 5 y 6
; Integrado con autenticador sb-acceso
; =====================================================================

[Setup]
AppId={{D37F3891-B789-4DF5-A9D4-29E7F6A52C01}
AppName=Calculadora HP71B
AppVersion=2.0
AppPublisher=Santa Bárbara
DefaultDirName={localappdata}\Programs\HP71_Ballistic
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=Calculadora_HP71B_Installer
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\icon.ico
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Lanzadores y scripts
Source: "Iniciar_HP71.vbs"; DestDir: "{app}"; Flags: ignoreversion
Source: "Iniciar_HP71.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "Iniciar_HP71.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "set_icon.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "splash.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "Splash_screen_hp71b.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "tables.csv"; DestDir: "{app}"; Flags: ignoreversion

; Binarios ejecutables (desde raíz o desde build)
Source: "hp71_emulator.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\hp71_server\Release\hp71_emulator.exe"; DestDir: "{app}"; DestName: "hp71_emulator.exe"; Flags: ignoreversion skipifsourcedoesntexist
Source: "splash.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Interfaz Web (calculadora, record de tiro, estilos e interactividad)
Source: "web_ui\*"; DestDir: "{app}\web_ui"; Flags: ignoreversion recursesubdirs createallsubdirs

; Tablas balísticas adicionales
Source: "tables\*"; DestDir: "{app}\tables"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Calculadora HP71B"; Filename: "{sys}\wscript.exe"; Parameters: """{app}\Iniciar_HP71.vbs"""; IconFilename: "{app}\icon.ico"
Name: "{autodesktop}\Calculadora HP71B"; Filename: "{sys}\wscript.exe"; Parameters: """{app}\Iniciar_HP71.vbs"""; IconFilename: "{app}\icon.ico"; Tasks: desktopicon

[Run]
Filename: "{sys}\wscript.exe"; Parameters: """{app}\Iniciar_HP71.vbs"""; Description: "{cm:LaunchProgram,Calculadora HP71B}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\record.json"
Type: files; Name: "{app}\hp71_emulator.log"
Type: filesandordirs; Name: "{app}\logs"
