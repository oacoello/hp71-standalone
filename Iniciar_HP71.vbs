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

' ===== BUILD URL WITH TOKEN =====
Dim appUrl, queryParams
appUrl = "http://127.0.0.1:8080"
queryParams = ""

If sessionToken <> "" Then
    queryParams = "sessionToken=" & sessionToken
End If
If deviceId <> "" Then
    If queryParams <> "" Then queryParams = queryParams & "&"
    queryParams = queryParams & "deviceId=" & deviceId
End If
If applicationCode <> "" Then
    If queryParams <> "" Then queryParams = queryParams & "&"
    queryParams = queryParams & "applicationCode=" & applicationCode
End If

If queryParams <> "" Then
    appUrl = appUrl & "?" & queryParams
End If

' ===== INSTANT SPLASH SCREEN =====
Dim splashExe
splashExe = appDir & "\splash.exe"
If fso.FileExists(splashExe) Then
    WshShell.Run """" & splashExe & """", 0, False
End If

' ===== LAUNCH APP =====
' Terminate previous emulator instance quickly
WshShell.Run "taskkill /F /IM hp71_emulator.exe", 0, True

' Launch emulator hidden
Dim exePath
exePath = appDir & "\hp71_emulator.exe"
If fso.FileExists(exePath) Then
    WshShell.CurrentDirectory = appDir
    WshShell.Run """" & exePath & """", 0, False
End If

' Micro-pause for socket initialization (< 20ms)
WScript.Sleep 50

' ===== OPEN BROWSER (launches behind TopMost splash) =====
Dim edgeApp, chromeApp
edgeApp = WshShell.ExpandEnvironmentStrings("%ProgramFiles(x86)%") & "\Microsoft\Edge\Application\msedge.exe"
chromeApp = WshShell.ExpandEnvironmentStrings("%ProgramFiles%") & "\Google\Chrome\Application\chrome.exe"

If fso.FileExists(edgeApp) Then
    WshShell.Run """" & edgeApp & """ --app=""" & appUrl & """", 1, False
ElseIf fso.FileExists(chromeApp) Then
    WshShell.Run """" & chromeApp & """ --app=""" & appUrl & """", 1, False
Else
    WshShell.Run """" & appUrl & """", 1, False
End If

' ===== KEEP ALIVE =====
On Error Resume Next
Dim svc
Set svc = GetObject("winmgmts:\\.\root\cimv2")
Do
    WScript.Sleep 2000
    Dim alive, procs, p
    alive = False
    Set procs = svc.ExecQuery("SELECT ProcessId FROM Win32_Process WHERE Name = 'hp71_emulator.exe'")
    For Each p in procs
        alive = True
    Next
    If Not alive Then Exit Do
Loop
