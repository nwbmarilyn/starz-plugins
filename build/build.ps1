param(
    [ValidateSet('vocal', 'reverb', 'delay', 'swarm', 'fit', 'all')][string]$Plugin = 'all',
    [switch]$Install,
    [switch]$Tests,
    [string]$Sdk = $(if ($env:VST3_SDK) { $env:VST3_SDK } else { 'D:\class FLPjcts\сведение\_работа\toolchain\vst3sdk' }),
    [string]$Zig = $(if ($env:ZIG_PY) { $env:ZIG_PY } else { 'D:\class FLPjcts\сведение\_работа\toolchain\venv\Scripts\python.exe' }),
    [string]$Dest = 'C:\Program Files\Common Files\VST3'
)
$ErrorActionPreference = 'Continue'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here
$out = Join-Path $here 'out'
$inc = @("-I$root\src\common", "-I$root\src\core", "-I$root\src\vocal", "-I$root\src\fx", "-I$Sdk")

$targets = @{
    fit    = @{ Name = 'StarzFit'; Main = "$root\src\fit\plugin.cpp"; Def = @() }
    swarm  = @{ Name = 'StarzSwarm';  Main = "$root\src\swarm\plugin.cpp"; Def = @() }
    vocal  = @{ Name = 'StarzVocal';  Main = "$root\src\vocal\plugin.cpp";  Def = @() }
    reverb = @{ Name = 'StarzReverb'; Main = "$root\src\fx\fxplugin.cpp";   Def = @('-DARK_REVERB') }
    delay  = @{ Name = 'StarzDelay';  Main = "$root\src\fx\fxplugin.cpp";   Def = @('-DARK_DELAY') }
}

$sdkSources = @(
    "base\source\baseiids.cpp", "base\source\fbuffer.cpp", "base\source\fdebug.cpp", "base\source\fdynlib.cpp",
    "base\source\fobject.cpp", "base\source\fstreamer.cpp", "base\source\fstring.cpp", "base\source\updatehandler.cpp",
    "base\thread\source\fcondition.cpp", "base\thread\source\flock.cpp",
    "pluginterfaces\base\conststringtable.cpp", "pluginterfaces\base\coreiids.cpp", "pluginterfaces\base\funknown.cpp", "pluginterfaces\base\ustring.cpp",
    "public.sdk\source\common\commoniids.cpp", "public.sdk\source\common\memorystream.cpp", "public.sdk\source\common\pluginview.cpp",
    "public.sdk\source\main\dllmain.cpp", "public.sdk\source\main\pluginfactory.cpp",
    "public.sdk\source\vst\vstaudioeffect.cpp", "public.sdk\source\vst\vstbus.cpp", "public.sdk\source\vst\vstcomponent.cpp",
    "public.sdk\source\vst\vstcomponentbase.cpp", "public.sdk\source\vst\vstinitiids.cpp",
    "public.sdk\source\vst\vstparameters.cpp", "public.sdk\source\vst\vstsinglecomponenteffect.cpp"
) | ForEach-Object { Join-Path $Sdk $_ }

function Build-One($key) {
    $t = $targets[$key]
    $obj = Join-Path $here ('obj_' + $t.Name)
    New-Item -ItemType Directory -Force $obj, $out | Out-Null
    $dst = Join-Path $out ($t.Name + '.vst3')
    Remove-Item $dst -ErrorAction SilentlyContinue
    $objs = @()
    $fail = 0
    $i = 0
    foreach ($s in (@($t.Main) + $sdkSources)) {
        $o = Join-Path $obj (($i++).ToString() + '_' + [IO.Path]::GetFileNameWithoutExtension($s) + '.o')
        $isOwn = $s.StartsWith($root)
        if (-not $isOwn -and (Test-Path $o) -and ((Get-Item $o).LastWriteTime -gt (Get-Item $s).LastWriteTime)) { $objs += $o; continue }
        Remove-Item $o -ErrorAction SilentlyContinue
        & $Zig -m ziglang c++ -target x86_64-windows-gnu -O2 -std=c++17 -DRELEASE=1 -DNDEBUG @($t.Def) -D_CRT_SECURE_NO_WARNINGS -w @inc -c $s -o $o 2>&1 | Select-Object -First 12 | Out-Host
        if (Test-Path $o) { $objs += $o } else { $fail++; Write-Host "FAILED: $s" }
    }
    if ($fail -gt 0) { Write-Host "compile failures: $fail"; return $false }
    & $Zig -m ziglang c++ -target x86_64-windows-gnu -shared -o $dst $objs -lole32 -luuid -lshell32 -ladvapi32 -luser32 -lgdi32 -lcomdlg32 -lgdiplus -lshlwapi 2>&1 | Select-Object -First 25 | Out-Host
    if (Test-Path $dst) { Write-Host ("BUILD OK: " + $t.Name + " " + (Get-Item $dst).Length + " bytes"); return $true }
    Write-Host "LINK FAILED: $($t.Name)"
    return $false
}

$keys = if ($Plugin -eq 'all') { @('vocal', 'reverb', 'delay', 'swarm', 'fit') } else { @($Plugin) }
$ok = $true
foreach ($k in $keys) { if (-not (Build-One $k)) { $ok = $false } }

if ($Tests) {
    $dstT = 'D:\arktest'
    New-Item -ItemType Directory -Force $dstT | Out-Null
    & $Zig -m ziglang c++ -target x86_64-windows-gnu -O2 -std=c++17 -w @inc "$root\tests\test_main.cpp" -o "$dstT\ark_test.exe" 2>&1 | Select-Object -First 8
    & $Zig -m ziglang c++ -target x86_64-windows-gnu -O2 -std=c++17 -w @inc "$root\tests\fx_test.cpp" -o "$dstT\fx_test.exe" 2>&1 | Select-Object -First 8
    "tests built in $dstT"
}

if ($Install -and $ok) {
    if (Get-Process FL64 -ErrorAction SilentlyContinue) { 'FL открыт: установка пропущена, закрой FL' }
    else {
        foreach ($old in 'ArkhamVocal', 'ArkhamReverb', 'ArkhamDelay') {
            $p = Join-Path $Dest ($old + '.vst3')
            if (Test-Path $p) { Remove-Item $p -Force; "удалён старый $old" }
        }
        foreach ($k in $keys) {
            $n = $targets[$k].Name
            Copy-Item (Join-Path $out ($n + '.vst3')) (Join-Path $Dest ($n + '.vst3')) -Force
            "установлен $n"
        }
    }
}
