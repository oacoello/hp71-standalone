Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
Dim appDir
appDir = fso.GetParentFolderName(WScript.ScriptFullName)

' ===== READ LAUNCH ARGS (from sb-acceso) =====
Dim sessionToken, deviceId, applicationCode
sessionToken = ""
deviceId = ""
applicationCode = ""

Dim i, arg
For i = 0 To WScript.Arguments.Count - 1
    arg = WScript.Arguments(i)
    If Left(LCase(arg), 15) = "--sessiontoken=" Then
        sessionToken = Mid(arg, 16)
    ElseIf Left(LCase(arg), 13) = "--session-token=" Then
        sessionToken = Mid(arg, 14)
    ElseIf Left(LCase(arg), 11) = "--deviceid=" Then
        deviceId = Mid(arg, 12)
    ElseIf Left(LCase(arg), 11) = "--device-id=" Then
        deviceId = Mid(arg, 12)
    ElseIf Left(LCase(arg), 18) = "--applicationcode=" Then
        applicationCode = Mid(arg, 19)
    ElseIf Left(LCase(arg), 18) = "--application-code=" Then
        applicationCode = Mid(arg, 19)
    End If
Next

' ===== SPLASH SCREEN =====
Dim splashPs, splashGif
splashPs = appDir & "\splash.ps1"
splashGif = appDir & "\Splash.gif"

If fso.FileExists(splashPs) And fso.FileExists(splashGif) Then
    WshShell.Run "powershell -NoProfile -ExecutionPolicy Bypass -File """ & splashPs & """ -GifPath """ & splashGif & """", 0, True
End If

' ===== LAUNCH APP =====
' Kill previous exe
On Error Resume Next
Dim svc
Set svc = GetObject("winmgmts:\\.\root\cimv2")
Dim procs, p
Set procs = svc.ExecQuery("SELECT ProcessId FROM Win32_Process WHERE Name = 'hp71_emulator.exe'")
For Each p in procs
    svc.Get("Win32_Process.Handle=" & p.ProcessId).Terminate
Next
On Error GoTo 0
WScript.Sleep 300

' Launch exe hidden
WshShell.CurrentDirectory = appDir
WshShell.Run "hp71_emulator.exe", 0, False
WScript.Sleep 1500

' ===== BUILD URL WITH TOKEN =====
Dim appUrl
appUrl = "http://127.0.0.1:8080"

If sessionToken <> "" Then
    appUrl = appUrl & "?sessionToken=" & sessionToken
    If deviceId <> "" Then appUrl = appUrl & "&deviceId=" & deviceId
    If applicationCode <> "" Then appUrl = appUrl & "&applicationCode=" & applicationCode
End If

' ===== OPEN BROWSER =====
Dim edgeApp
edgeApp = WshShell.ExpandEnvironmentStrings("%ProgramFiles(x86)%") & "\Microsoft\Edge\Application\msedge.exe"
If fso.FileExists(edgeApp) Then
    WshShell.Run """" & edgeApp & """ --app=""" & appUrl & """", 1, False
Else
    Dim chromePath
    chromePath = WshShell.ExpandEnvironmentStrings("%ProgramFiles%") & "\Google\Chrome\Application\chrome.exe"
    If fso.FileExists(chromePath) Then
        WshShell.Run """" & chromePath & """ --app=""" & appUrl & """", 1, False
    Else
        WshShell.Run appUrl, 1, False
    End If
End If

' ===== SET TASKBAR ICON =====
Dim setIconPs, iconFile
setIconPs = appDir & "\set_icon.ps1"
iconFile = appDir & "\icon.ico"
If fso.FileExists(setIconPs) And fso.FileExists(iconFile) Then
    WshShell.Run "powershell -NoProfile -ExecutionPolicy Bypass -File """ & setIconPs & """ -IconPath """ & iconFile & """", 0, False
End If

' ===== KEEP ALIVE =====
Do
    WScript.Sleep 2000
    Dim alive
    alive = False
    Set procs = svc.ExecQuery("SELECT Name FROM Win32_Process WHERE Name = 'hp71_emulator.exe'")
    For Each p in procs
        alive = True
    Next
    If Not alive Then Exit Do
Loop
