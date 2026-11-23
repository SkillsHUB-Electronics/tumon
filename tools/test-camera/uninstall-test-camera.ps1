# Removes everything install-test-camera.ps1 added. RUN IN AN ADMINISTRATOR POWERSHELL.
#   1. unregisters the source (deletes HKLM\Software\Classes\CLSID\{38035696-...})
#   2. deletes C:\Program Files\Tumon\tumon_source.dll (and the folder if it is empty)
# Stop tumon_vcam_host.exe first (press Enter in its window) so the camera is gone before the DLL is removed.
$ErrorActionPreference = 'Stop'

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { throw 'Run this script in an Administrator PowerShell (right-click PowerShell, Run as administrator).' }

if (Get-Process -Name tumon_vcam_host -ErrorAction SilentlyContinue) {
    throw 'tumon_vcam_host.exe is still running. Stop it first (press Enter in its window), then run this again.'
}

$destination = Join-Path $env:ProgramFiles 'Tumon'
$dll = Join-Path $destination 'tumon_source.dll'
$ctl = Join-Path $destination 'tumon_vcam_ctl.exe'

# A permanent camera (install-test-camera.ps1 -Permanent) must be removed first. Apps using it are told the device
# went away. Harmless when there is no such camera.
if (Test-Path $ctl) {
    & $ctl remove
    Start-Sleep -Seconds 1
}

if (Test-Path $dll) {
    $process = Start-Process -FilePath "$env:WINDIR\System32\regsvr32.exe" -ArgumentList '/u', '/s', "`"$dll`"" -Wait -PassThru
    if ($process.ExitCode -ne 0) { Write-Warning "regsvr32 /u returned $($process.ExitCode); removing the registry key directly." }
}
$key = 'HKLM:\SOFTWARE\Classes\CLSID\{38035696-DAA4-41E0-95BC-C1FC73185CFE}'
if (Test-Path $key) { Remove-Item -Recurse -Force $key }

# The Windows Frame Server services keep the DLL loaded after the camera is gone, so deleting it can fail with
# "access denied". Stop them briefly, delete, and start them again (they also start on demand).
if (Test-Path $dll) {
    try {
        Remove-Item -Force $dll -ErrorAction Stop
    } catch {
        Write-Output 'The DLL is still loaded by the Windows Frame Server; stopping it for a moment...'
        Stop-Service -Name FrameServerMonitor -Force -ErrorAction SilentlyContinue
        Stop-Service -Name FrameServer -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
        try {
            Remove-Item -Force $dll -ErrorAction Stop
        } finally {
            Start-Service -Name FrameServer -ErrorAction SilentlyContinue
            Start-Service -Name FrameServerMonitor -ErrorAction SilentlyContinue
        }
    }
}
if (Test-Path $ctl) { Remove-Item -Force $ctl }
if ((Test-Path $destination) -and -not (Get-ChildItem $destination -Force)) { Remove-Item -Force $destination }

Write-Output "Removed. Registry key present: $(Test-Path $key)"
Write-Output "DLL present: $(Test-Path $dll)"
