[Setup]
AppId={{D37F3891-B789-4DF5-A9D4-29E7F6A52C01}
AppName=Calculadora HP71B
AppVersion=2.0
AppPublisher=Santa Bárbara
DefaultDirName={localappdata}\Programs\HP71_Ballistic
DisableProgramGroupPage=yes
OutputDir=C:\Users\oacoe\Desktop
OutputBaseFilename=Calculadora_HP71B_Installer
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
SetupIconFile=C:\Users\oacoe\My Docs\Projects\hp71-standalone\icon.ico
UninstallDisplayIcon={app}\icon.ico

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Files]
Source: "C:\Users\oacoe\My Docs\Projects\hp71-standalone\Iniciar_HP71.vbs"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\oacoe\My Docs\Projects\hp71-standalone\hp71_emulator.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\oacoe\My Docs\Projects\hp71-standalone\tables.csv"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\oacoe\My Docs\Projects\hp71-standalone\Splash_screen_hp71b.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\oacoe\My Docs\Projects\hp71-standalone\splash.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\oacoe\My Docs\Projects\hp71-standalone\icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\oacoe\My Docs\Projects\hp71-standalone\web_ui\*"; DestDir: "{app}\web_ui"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Calculadora HP71B"; Filename: "{sys}\wscript.exe"; Parameters: """{app}\Iniciar_HP71.vbs"""; IconFilename: "{app}\icon.ico"
Name: "{autodesktop}\Calculadora HP71B"; Filename: "{sys}\wscript.exe"; Parameters: """{app}\Iniciar_HP71.vbs"""; IconFilename: "{app}\icon.ico"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
