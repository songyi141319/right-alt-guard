param([switch]$DesktopTests)
$ErrorActionPreference = 'Stop'
$project = $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Install Visual Studio C++ Build Tools and a Windows SDK to build from source.' }
$vc = Get-ChildItem -LiteralPath "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdk = Get-ChildItem -LiteralPath "$sdkRoot\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$includes = @("/I$($vc.FullName)\include", "/I$sdkRoot\Include\$($sdk.Name)\ucrt", "/I$sdkRoot\Include\$($sdk.Name)\shared", "/I$sdkRoot\Include\$($sdk.Name)\um")
$flags = @('/nologo','/std:c++17','/W4','/WX','/EHsc','/MT','/utf-8','/DUNICODE','/D_UNICODE')
$dist = Join-Path $project 'dist'
New-Item -ItemType Directory -Path $dist -Force | Out-Null
Push-Location $project
try {
    & (Join-Path $project 'tools\create-icons.ps1')
    foreach ($arch in @('x64','x86')) {
        $out = Join-Path $project "build\$arch"
        New-Item -ItemType Directory -Path $out -Force | Out-Null
        $compiler = Join-Path $vc.FullName "bin\Hostx64\$arch\cl.exe"
        $libs = @("/LIBPATH:$($vc.FullName)\lib\$arch", "/LIBPATH:$sdkRoot\Lib\$($sdk.Name)\ucrt\$arch", "/LIBPATH:$sdkRoot\Lib\$($sdk.Name)\um\$arch")
        & $compiler @flags @includes 'tests\menu_policy_test.cpp' "/Fo$out\policy-test.obj" "/Fe$out\policy-test.exe" /link /Brepro @libs
        if ($LASTEXITCODE -ne 0) { throw "Policy test compile failed: $arch" }
        & "$out\policy-test.exe"
        if ($LASTEXITCODE -ne 0) { throw "Policy test failed: $arch" }
        & $compiler @flags /LD @includes 'src\menu_guard.cpp' "/Fo$out\menu-guard.obj" "/Fe$out\MenuGuard-v2-$arch.dll" /link /Brepro @libs user32.lib
        if ($LASTEXITCODE -ne 0) { throw "Component compile failed: $arch" }
        & $compiler @flags @includes 'src\host.cpp' "/Fo$out\host.obj" "/Fe$out\RightAltGuard-v2.exe" /link /Brepro /SUBSYSTEM:WINDOWS @libs user32.lib shell32.lib
        if ($LASTEXITCODE -ne 0) { throw "Worker compile failed: $arch" }
        foreach ($test in @('app_menu_fixture','native_fixture')) {
            & $compiler @flags @includes "tests\$test.cpp" "/Fo$out\$test.obj" "/Fe$out\$test.exe" /link /Brepro @libs user32.lib
            if ($LASTEXITCODE -ne 0) { throw "Fixture compile failed: $arch/$test" }
        }
        & "$out\app_menu_fixture.exe"
        if ($LASTEXITCODE -ne 0) { throw "App-menu test failed: $arch" }
    }
    $out = Join-Path $project 'build\x64'
    $libs = @("/LIBPATH:$($vc.FullName)\lib\x64", "/LIBPATH:$sdkRoot\Lib\$($sdk.Name)\ucrt\x64", "/LIBPATH:$sdkRoot\Lib\$($sdk.Name)\um\x64")
    $resourceCompiler = Join-Path $sdkRoot "bin\$($sdk.Name)\x64\rc.exe"
    & $resourceCompiler /nologo @includes "/fo$out\payload.res" 'src\payload.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Resource packing failed.' }
    & "$($vc.FullName)\bin\Hostx64\x64\cl.exe" @flags @includes 'src\launcher.cpp' 'src\payload.cpp' "/Fo$out\\" "/Fe$dist\RightAltGuard.exe" /link /Brepro /SUBSYSTEM:WINDOWS @libs "$out\payload.res" user32.lib shell32.lib ole32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Single-file launcher compile failed.' }
    & 'tests\package_test.ps1' -Executable "$dist\RightAltGuard.exe"
    if ($DesktopTests) {
        & 'tests\tray_test.ps1' -Executable "$dist\RightAltGuard.exe"
        foreach ($arch in @('x64','x86')) {
            & "build\$arch\native_fixture.exe"
            if ($LASTEXITCODE -ne 0) { throw "Native desktop test failed: $arch" }
        }
    }
    $hash = (Get-FileHash -LiteralPath "$dist\RightAltGuard.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText("$dist\SHA256SUMS.txt", "$hash  RightAltGuard.exe`n", [Text.UTF8Encoding]::new($false))
    Write-Output "Built: $dist\RightAltGuard.exe"
} finally { Pop-Location }
