Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
Dim appDir
appDir = fso.GetParentFolderName(WScript.ScriptFullName)

' ===== READ LAUNCH ARGS (from sb-acceso) =====
Dim sessionToken, deviceId, applicationCode
sessionToken = ""
deviceId = ""
applicationCode = ""

Dim i, arg, nextArg, argLower
For i = 0 To WScript.Arguments.Count - 1
    arg = WScript.Arguments(i)
    argLower = LCase(arg)
    nextArg = ""
    If i < WScript.Arguments.Count - 1 Then nextArg = WScript.Arguments(i + 1)

    If Left(argLower, 15) = "--sessiontoken=" Then
        sessionToken = Mid(arg, 16)
    ElseIf Left(argLower, 16) = "--session_token=" Then
        sessionToken = Mid(arg, 17)
    ElseIf Left(argLower, 13) = "--session-token=" Then
        sessionToken = Mid(arg, 14)
    ElseIf Left(argLower, 8) = "--token=" Then
        sessionToken = Mid(arg, 9)
    ElseIf argLower = "--sessiontoken" Or argLower = "--session-token" Or argLower = "--session_token" Or argLower = "--token" Then
        If nextArg <> "" Then sessionToken = nextArg
    ElseIf Left(argLower, 11) = "--deviceid=" Then
        deviceId = Mid(arg, 12)
    ElseIf Left(argLower, 11) = "--device-id=" Then
        deviceId = Mid(arg, 12)
    ElseIf Left(argLower, 11) = "--device_id=" Then
        deviceId = Mid(arg, 12)
    ElseIf argLower = "--deviceid" Or argLower = "--device-id" Or argLower = "--device_id" Then
        If nextArg <> "" Then deviceId = nextArg
    ElseIf Left(argLower, 18) = "--applicationcode=" Then
        applicationCode = Mid(arg, 19)
    ElseIf Left(argLower, 18) = "--application-code=" Then
        applicationCode = Mid(arg, 19)
    ElseIf Left(argLower, 18) = "--application_code=" Then
        applicationCode = Mid(arg, 19)
    ElseIf argLower = "--applicationcode" Or argLower = "--application-code" Or argLower = "--application_code" Then
        If nextArg <> "" Then applicationCode = nextArg
    End If
Next

sessionToken = Trim(Replace(sessionToken, """", ""))
deviceId = Trim(Replace(deviceId, """", ""))
applicationCode = Trim(Replace(applicationCode, """", ""))

' ===== VALIDAR TOKEN DE SB-ACCESO =====
If sessionToken = "" Then
    MsgBox "Acceso restringido." & vbCrLf & vbCrLf & _
           "La Calculadora HP-71B debe ejecutarse exclusivamente desde el autenticador sb-acceso." & vbCrLf & vbCrLf & _
           "Por favor, inicie sesión en sb-acceso y presione el botón 'Abrir' correspondiente a la Calculadora.", _
           vbCritical, "Acceso Restringido - Santa Bárbara"
    WScript.Quit 0
End If

If applicationCode = "" Then applicationCode = "calc"

' ===== INSTANT SPLASH SCREEN =====
Dim splashExe, splashPs, splashImg
splashExe = appDir & "\splash.exe"
splashPs = appDir & "\splash.ps1"
splashImg = appDir & "\Splash_screen_hp71b.png"

If fso.FileExists(splashExe) Then
    WshShell.Run """" & splashExe & """", 0, False
ElseIf fso.FileExists(splashPs) And fso.FileExists(splashImg) Then
    WshShell.Run "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & splashPs & """ -ImagePath """ & splashImg & """", 0, False
End If

' ===== TERMINATE PREVIOUS EMULATOR =====
On Error Resume Next
WshShell.Run "taskkill /F /IM hp71_emulator.exe", 0, True
On Error GoTo 0
WScript.Sleep 200

' ===== LAUNCH EMULATOR (HIDDEN) =====
Dim exePath
exePath = appDir & "\hp71_emulator.exe"
If fso.FileExists(exePath) Then
    WshShell.CurrentDirectory = appDir
    WshShell.Run """" & exePath & """", 0, False
Else
    MsgBox "No se encontro hp71_emulator.exe en: " & vbCrLf & appDir, vbCritical, "Error Calculadora HP71B"
    WScript.Quit 1
End If

' Pausa breve para enlace del socket HTTP en puerto 8071
WScript.Sleep 600

' ===== BUILD URL WITH SB-ACCESO TOKENS =====
Dim appUrl, queryParams
appUrl = "http://127.0.0.1:8071"
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

' ===== OPEN BROWSER (EDGE / CHROME EN MODO APP) =====
Dim edgePath1, edgePath2, chromePath1, chromePath2, browserPath
edgePath1 = WshShell.ExpandEnvironmentStrings("%ProgramFiles(x86)%") & "\Microsoft\Edge\Application\msedge.exe"
edgePath2 = WshShell.ExpandEnvironmentStrings("%ProgramFiles%") & "\Microsoft\Edge\Application\msedge.exe"
chromePath1 = WshShell.ExpandEnvironmentStrings("%ProgramFiles%") & "\Google\Chrome\Application\chrome.exe"
chromePath2 = WshShell.ExpandEnvironmentStrings("%ProgramFiles(x86)%") & "\Google\Chrome\Application\chrome.exe"
browserPath = ""

If fso.FileExists(edgePath1) Then
    browserPath = edgePath1
ElseIf fso.FileExists(edgePath2) Then
    browserPath = edgePath2
ElseIf fso.FileExists(chromePath1) Then
    browserPath = chromePath1
ElseIf fso.FileExists(chromePath2) Then
    browserPath = chromePath2
End If

If browserPath <> "" Then
    WshShell.Run """" & browserPath & """ --app=""" & appUrl & """", 1, False
Else
    WshShell.Run """" & appUrl & """", 1, False
End If

' ===== APLICAR ICONO NÍTIDO EN TASKBAR =====
Dim setIconPs, iconFile
setIconPs = appDir & "\set_icon.ps1"
iconFile = appDir & "\icon.ico"
If fso.FileExists(setIconPs) And fso.FileExists(iconFile) Then
    WshShell.Run "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & setIconPs & """ -IconPath """ & iconFile & """", 0, False
End If

' ===== SUPERVISIÓN EN BACKGROUND (KEEP-ALIVE) =====
On Error Resume Next
Dim svc, procs, p, alive
Set svc = GetObject("winmgmts:\\.\root\cimv2")
Do
    WScript.Sleep 2500
    alive = False
    Set procs = svc.ExecQuery("SELECT ProcessId FROM Win32_Process WHERE Name = 'hp71_emulator.exe'")
    For Each p in procs
        alive = True
    Next
    If Not alive Then Exit Do
Loop
