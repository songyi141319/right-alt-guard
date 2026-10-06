param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$testRoot = Join-Path $project ('build\package-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$singleExe = Join-Path $testRoot 'RightAltGuard.exe'
Copy-Item -LiteralPath $Executable -Destination $singleExe
$payload = Join-Path $testRoot 'payload'
function Invoke-Extract {
    $process = Start-Process -FilePath $singleExe -ArgumentList @('--extract-only',('"' + $payload + '"')) -WindowStyle Hidden -Wait -PassThru
    try { return $process.ExitCode } finally { $process.Dispose() }
}
if ((Invoke-Extract) -ne 0) { throw 'FAIL: a single executable must unpack its own required components without scripts or adjacent DLLs.' }
$expected = @('MenuGuard-v2-x64.dll','MenuGuard-v2-x86.dll','RightAltGuard-v2-x64.exe','RightAltGuard-v2-x86.exe')
$actual = @(Get-ChildItem -LiteralPath $payload -File | Select-Object -ExpandProperty Name | Sort-Object)
if (@(Compare-Object ($expected | Sort-Object) $actual).Count) { throw 'FAIL: extracted payload is incomplete or contains unexpected files.' }
foreach ($file in $expected) {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $payload $file))
    $pe = [BitConverter]::ToInt32($bytes,60)
    $machine = [BitConverter]::ToUInt16($bytes,$pe+4)
    $wanted = if ($file.Contains('x64')) { 0x8664 } else { 0x014c }
    if ($machine -ne $wanted) { throw "FAIL: wrong machine architecture: $file" }
}
$before = @(Get-ChildItem -LiteralPath $payload -File | ForEach-Object { $_.LastWriteTimeUtc.Ticks })
if ((Invoke-Extract) -ne 0) { throw 'FAIL: repeated extraction of matching components must work.' }
$after = @(Get-ChildItem -LiteralPath $payload -File | ForEach-Object { $_.LastWriteTimeUtc.Ticks })
if (@(Compare-Object $before $after).Count) { throw 'FAIL: matching existing components must not be rewritten.' }
$changedFile = Join-Path $payload 'MenuGuard-v2-x64.dll'
$changed = [IO.File]::ReadAllBytes($changedFile)
$changed[128] = $changed[128] -bxor 1
[IO.File]::WriteAllBytes($changedFile,$changed)
if ((Invoke-Extract) -eq 0) { throw 'FAIL: a mismatched component must fail closed instead of being loaded or overwritten.' }
if ([IO.File]::ReadAllBytes($changedFile)[128] -ne $changed[128]) { throw 'FAIL: the mismatched file must remain untouched.' }
$target = Join-Path $testRoot 'junction-target'
$junction = Join-Path $testRoot 'junction'
New-Item -ItemType Directory -Path $target -Force | Out-Null
New-Item -ItemType Junction -Path $junction -Target $target | Out-Null
$payload = Join-Path $junction 'must-not-be-created'
if ((Invoke-Extract) -eq 0) { throw 'FAIL: a parent directory junction must be refused.' }
if (@(Get-ChildItem -LiteralPath $target -Force).Count) { throw 'FAIL: refusing a parent junction must not create anything in its target.' }
$payload = $junction
if ((Invoke-Extract) -eq 0) { throw 'FAIL: a final directory junction must be refused.' }
if (@(Get-ChildItem -LiteralPath $target -Force).Count) { throw 'FAIL: refusing a final junction must leave its target untouched.' }
Write-Output 'PASS: single-file extraction, both architectures, unchanged reuse, mismatched-file refusal, and parent/final junction refusal.'
