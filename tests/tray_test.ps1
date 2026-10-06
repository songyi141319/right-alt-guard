param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class GuardTrayTest {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr FindWindow(string cls, string title);
  public static IntPtr Window() { return FindWindow("RightAltGuard.Tray.v1", null); }
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint process);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr command, IntPtr data);
}
'@
$exe = (Resolve-Path -LiteralPath $Executable).Path
if ([GuardTrayTest]::Window() -ne [IntPtr]::Zero) { throw 'Exit the running RightAlt Guard tray before this test.' }
$child = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
$ownedController = $false
function Command([string]$argument) {
    $commandProcess = Start-Process -FilePath $exe -ArgumentList $argument -WindowStyle Hidden -Wait -PassThru
    try { return $commandProcess.ExitCode } finally { $commandProcess.Dispose() }
}
function Workers([int]$controller) {
    $workers = @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $controller" -Property ProcessId,Name |
        Where-Object { $_.Name -in @('RightAltGuard-v2-x64.exe','RightAltGuard-v2-x86.exe') } |
        ForEach-Object { [Diagnostics.Process]::GetProcessById($_.ProcessId) })
    if ($workers.Count -ne 2) { throw 'FAIL: exactly two workers must belong to this controller.' }
    return $workers
}
function Gone($workers) {
    foreach ($worker in $workers) {
        try { if (-not $worker.WaitForExit(3000)) { throw "FAIL: worker $($worker.Id) remained running." } }
        finally { $worker.Dispose() }
    }
}
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        $window = [GuardTrayTest]::Window()
        if ($child.HasExited) { break }
        if ($window -ne [IntPtr]::Zero -and (Command '--status') -eq 0) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($child.HasExited -or $window -eq [IntPtr]::Zero) { throw 'FAIL: double-click startup must remain running with an accessible tray controller.' }
    [uint32]$owner = 0
    [void][GuardTrayTest]::GetWindowThreadProcessId($window,[ref]$owner)
    if ($owner -ne $child.Id) { throw 'FAIL: expected a clean tray baseline before this test.' }
    $ownedController = $true
    if ((Command '--status') -ne 0) { throw 'FAIL: both background architectures must be ready.' }
    $workers = @(Workers $child.Id)
    $duplicate = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
    try {
        if (-not $duplicate.WaitForExit(3000) -or $duplicate.ExitCode -ne 0) { throw 'FAIL: repeated double-click must exit quietly.' }
    } finally { $duplicate.Dispose() }
    [void][GuardTrayTest]::SendMessage($window,0x0111,[IntPtr]1001,[IntPtr]::Zero)
    if ((Command '--status') -eq 0) { throw 'FAIL: Pause menu must stop protection.' }
    Gone $workers
    [void][GuardTrayTest]::SendMessage($window,0x0111,[IntPtr]1001,[IntPtr]::Zero)
    if ((Command '--status') -ne 0) { throw 'FAIL: Resume menu must restore protection.' }
    $workers = @(Workers $child.Id)
    $workers[0].Kill()
    if (-not $workers[0].WaitForExit(3000) -or -not $workers[1].WaitForExit(5000)) {
        throw 'FAIL: loss of one worker must automatically stop the remaining worker.'
    }
    Gone $workers
    if ((Command '--status') -eq 0) { throw 'FAIL: protection must be off after a worker failure.' }
    [void][GuardTrayTest]::SendMessage($window,0x0111,[IntPtr]1001,[IntPtr]::Zero)
    if ((Command '--status') -ne 0) { throw 'FAIL: Resume must recover after a worker failure.' }
    $workers = @(Workers $child.Id)
    if ((Command '--stop') -ne 0 -or -not $child.WaitForExit(8000)) { throw 'FAIL: Exit must stop the tray and the background protection.' }
    if ((Command '--status') -eq 0) { throw 'FAIL: protection must be off after exit.' }
    Gone $workers
    Write-Output 'PASS: startup, duplicate launch, Pause, Resume, single-worker failure/recovery, and clean Exit.'
    $child.Dispose()
    $child = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    while ((Command '--status') -ne 0 -and -not $child.HasExited -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if ($child.HasExited -or (Command '--status') -ne 0) { throw 'FAIL: could not start the crash-cleanup check.' }
    $workers = @(Workers $child.Id)
    $child.Kill()
    if (-not $child.WaitForExit(3000)) { throw 'FAIL: the specific test controller did not exit.' }
    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    do {
        $ready = (Command '--status') -eq 0
        if (-not $ready) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($ready) { throw 'FAIL: a crashed controller must not leave the keyboard workers running.' }
    Gone $workers
    Write-Output 'PASS: forced controller exit automatically releases its workers.'
} finally {
    if ($ownedController) { [void](Command '--stop') }
    # Only the exact child created by this test is eligible for cleanup.
    if (-not $child.HasExited) {
        if ([GuardTrayTest]::Window() -ne [IntPtr]::Zero) { [void](Command '--stop') }
        if (-not $child.WaitForExit(3000)) { $child.Kill(); [void]$child.WaitForExit(3000) }
    }
    $child.Dispose()
}
