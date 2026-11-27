; Tumon installer (Inno Setup 6). Build the native code first (docs/building.md), then:
;   ISCC.exe /DBuildDir=<folder with Tumon.exe> packaging\windows\installer.iss
; The result is packaging\windows\Output\Tumon-Setup-<version>.exe (not code-signed yet).
;
; What it does: copies Tumon to C:\Program Files\Tumon (the Windows Frame Server must be able to read the DLL there),
; registers the camera source, adds "Tumon Camera" as a permanent camera, and creates a Start menu entry.
; Uninstalling reverses all of it.

#ifndef BuildDir
  #define BuildDir "..\..\cpp\build\Release"
#endif
#define AppVersion "0.1.0"

[Setup]
AppId={{585A3B91-543E-4E6B-A372-AF5326F5738A}
AppName=Tumon
AppVersion={#AppVersion}
AppPublisher=SkillsHUB Electronics
AppPublisherURL=https://github.com/SkillsHUB-Electronics/tumon
AppSupportURL=https://github.com/SkillsHUB-Electronics/tumon/issues
DefaultDirName={autopf}\Tumon
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; The virtual camera API exists from Windows 11 (build 22000).
MinVersion=10.0.22000
LicenseFile=..\..\LICENSE
OutputDir=Output
OutputBaseFilename=Tumon-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
AppMutex=Local\TumonApp
UninstallDisplayIcon={app}\Tumon.exe

[Tasks]
Name: desktopicon; Description: "Create a &desktop shortcut"; Flags: unchecked

[Files]
Source: "{#BuildDir}\Tumon.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\tumon_source.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 64bit
Source: "{#BuildDir}\tumon_vcam_ctl.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\..\PRIVACY.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Tumon"; Filename: "{app}\Tumon.exe"
Name: "{autodesktop}\Tumon"; Filename: "{app}\Tumon.exe"; Tasks: desktopicon

[Registry]
; "Open Tumon when I sign in" is written by the app; remove it when uninstalling.
; ISCC warns about this (UsedUserAreasWarning): HKCU here is the account that elevated the uninstaller. If someone
; else's administrator password was used, the other user's entry stays behind; it then points to a missing file and does
; nothing. Accepted for 0.1.0.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "Tumon"; Flags: dontcreatekey uninsdeletevalue

[Run]
Filename: "{app}\tumon_vcam_ctl.exe"; Parameters: "install"; StatusMsg: "Adding Tumon Camera..."; Flags: runhidden waituntilterminated
; Started through Explorer so Tumon never runs as administrator, even if Setup was launched from an administrator
; shell ("runasoriginaluser" does not help then). An elevated window is hard to use with a screen reader.
Filename: "{win}\explorer.exe"; Parameters: """{app}\Tumon.exe"""; Description: "Open Tumon"; Flags: nowait postinstall skipifsilent

[Code]
// The Windows Frame Server keeps the camera DLL loaded; stop it briefly so the file can be replaced or deleted.
procedure RunHidden(const FileName, Parameters: String);
var
  ResultCode: Integer;
begin
  Exec(FileName, Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

procedure StopFrameServer;
begin
  RunHidden(ExpandConstant('{sys}\net.exe'), 'stop FrameServerMonitor /y');
  RunHidden(ExpandConstant('{sys}\net.exe'), 'stop FrameServer /y');
end;

procedure StartFrameServer;
begin
  RunHidden(ExpandConstant('{sys}\net.exe'), 'start FrameServer');
  RunHidden(ExpandConstant('{sys}\net.exe'), 'start FrameServerMonitor');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  if FileExists(ExpandConstant('{app}\tumon_source.dll')) then
    StopFrameServer;
  Result := '';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    StartFrameServer;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    // Remove the camera while the DLL is still there, then release the file.
    RunHidden(ExpandConstant('{app}\tumon_vcam_ctl.exe'), 'remove');
    StopFrameServer;
  end;
  if CurUninstallStep = usPostUninstall then
    StartFrameServer;
end;
