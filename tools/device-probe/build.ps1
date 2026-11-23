# Builds the Tumon device probe with the C# compiler that ships with Windows (.NET Framework 4).
# No SDK or Visual Studio needed. Produces bin\probe-x64.exe and bin\probe-x86.exe.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path $csc)) { throw "csc.exe not found at $csc" }
$bin = Join-Path $here 'bin'
New-Item -ItemType Directory -Force $bin | Out-Null
foreach ($p in 'x64', 'x86') {
    & $csc /nologo /optimize+ /target:exe /platform:$p "/out:$bin\probe-$p.exe" (Join-Path $here 'Probe.cs')
    if ($LASTEXITCODE -ne 0) { throw "build failed for $p" }
    Write-Output "built $bin\probe-$p.exe"
}
