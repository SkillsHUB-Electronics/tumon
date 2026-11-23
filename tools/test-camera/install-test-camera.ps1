# Registers the Tumon test camera source on this PC. RUN IN AN ADMINISTRATOR POWERSHELL.
#
# What it changes (all undone by uninstall-test-camera.ps1):
#   1. copies tumon_source.dll to  C:\Program Files\Tumon\
#   2. registers it with regsvr32, which writes HKEY_LOCAL_MACHINE\Software\Classes\CLSID\{38035696-...}
#
# Why Program Files and HKLM: the Windows Frame Server services load the DLL, so it must be registered for
# the machine and sit in a folder those services can read (not under C:\Users\...).
#
# After this, start the camera with:  tumon_vcam_host.exe   (the camera exists while that program runs).
#
# With -Permanent it also makes "Tumon Camera" a permanent camera (system lifetime, all users): it is then always
# listed, even when no program runs and after restarts. Without it, you start the camera yourself with
# tumon_vcam_host.exe and it disappears when that program closes.
param(
    [string]$BuildDir,
    [switch]$Permanent
)
$ErrorActionPreference = 'Stop'

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { throw 'Run this script in an Administrator PowerShell (right-click PowerShell, Run as administrator).' }

if (-not $BuildDir) {
    $candidates = @(
        (Join-Path $PSScriptRoot '..\..\cpp\build\Release'),
        (Join-Path $env:USERPROFILE 'build\tumon\Release')
    )
    $BuildDir = $candidates | Where-Object { Test-Path (Join-Path $_ 'tumon_source.dll') } | Select-Object -First 1
}
if (-not $BuildDir -or -not (Test-Path (Join-Path $BuildDir 'tumon_source.dll'))) {
    throw 'tumon_source.dll not found. Build it first (see docs/building.md) or pass -BuildDir <folder that contains it>.'
}

$destination = Join-Path $env:ProgramFiles 'Tumon'
New-Item -ItemType Directory -Force $destination | Out-Null
$dll = Join-Path $destination 'tumon_source.dll'
$source = Join-Path $BuildDir 'tumon_source.dll'
$same = (Test-Path $dll) -and ((Get-FileHash $source).Hash -eq (Get-FileHash $dll).Hash)
if ($same) {
    Write-Output 'tumon_source.dll is already installed and up to date; not copying it again.'
} else {
    try {
        Copy-Item $source $dll -Force -ErrorAction Stop
    } catch {
        # The Windows Frame Server keeps the installed DLL loaded while the camera exists.
        Write-Output 'The installed DLL is in use by the Windows Frame Server; stopping it for a moment...'
        Stop-Service -Name FrameServerMonitor -Force -ErrorAction SilentlyContinue
        Stop-Service -Name FrameServer -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
        try {
            Copy-Item $source $dll -Force -ErrorAction Stop
        } finally {
            Start-Service -Name FrameServer -ErrorAction SilentlyContinue
            Start-Service -Name FrameServerMonitor -ErrorAction SilentlyContinue
        }
    }
}
Unblock-File $dll

$process = Start-Process -FilePath "$env:WINDIR\System32\regsvr32.exe" -ArgumentList '/s', "`"$dll`"" -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "regsvr32 failed with exit code $($process.ExitCode)" }

$key = 'HKLM:\SOFTWARE\Classes\CLSID\{38035696-DAA4-41E0-95BC-C1FC73185CFE}\InprocServer32'
if (-not (Test-Path $key)) { throw 'Registration did not create the expected registry key.' }

Write-Output "Installed: $dll"
Write-Output "Registered: $((Get-ItemProperty $key).'(default)')"

if ($Permanent) {
    $ctlSource = Join-Path $BuildDir 'tumon_vcam_ctl.exe'
    if (-not (Test-Path $ctlSource)) { throw 'tumon_vcam_ctl.exe not found next to tumon_source.dll. Build all targets (see docs/building.md).' }
    $ctl = Join-Path $destination 'tumon_vcam_ctl.exe'
    Copy-Item $ctlSource $ctl -Force
    & $ctl install
    if ($LASTEXITCODE -ne 0) { throw "tumon_vcam_ctl install failed (exit code $LASTEXITCODE)" }
    & $ctl status
    Write-Output ''
    Write-Output 'Tumon Camera is now a permanent camera. Open TeamTalk > Preferences > Video Capture and select it.'
} else {
    Write-Output ''
    Write-Output 'Next: in a normal PowerShell window run tumon_vcam_host.exe and leave it open while you test.'
    Write-Output '(Or run this script again with -Permanent to make the camera permanent.)'
}
Write-Output 'To undo everything: uninstall-test-camera.ps1 (administrator).'
