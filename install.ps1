param(
    [switch]$User,
    [switch]$Uninstall,
    [string]$Repo = 'nwbmarilyn/starz-plugins',
    [string]$Branch = 'main'
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$names = 'StarzVocal.vst3', 'StarzReverb.vst3', 'StarzDelay.vst3'
$old = 'ArkhamVocal.vst3', 'ArkhamReverb.vst3', 'ArkhamDelay.vst3'
$raw = "https://raw.githubusercontent.com/$Repo/$Branch"

$dest = if ($User) { Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3' } else { Join-Path $env:CommonProgramFiles 'VST3' }

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $User -and -not $isAdmin) {
    $self = $PSCommandPath
    if (-not $self) {
        $self = Join-Path $env:TEMP 'starz-install.ps1'
        Invoke-WebRequest "$raw/install.ps1" -OutFile $self -UseBasicParsing
    }
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$self`"", '-Repo', $Repo, '-Branch', $Branch)
    if ($Uninstall) { $argList += '-Uninstall' }
    Start-Process powershell -Verb RunAs -ArgumentList $argList -Wait
    return
}

if (Get-Process FL64, FL -ErrorAction SilentlyContinue) {
    Write-Host 'FL Studio открыт: закрой его и запусти установку снова.' -ForegroundColor Yellow
    return
}

New-Item -ItemType Directory -Force $dest | Out-Null

foreach ($n in $old) {
    $p = Join-Path $dest $n
    if (Test-Path $p) { Remove-Item $p -Force; Write-Host "удалён старый $n" }
}

if ($Uninstall) {
    foreach ($n in $names) {
        $p = Join-Path $dest $n
        if (Test-Path $p) { Remove-Item $p -Force; Write-Host "удалён $n" }
    }
    Write-Host 'Готово.'
    return
}

$local = if ($PSScriptRoot) { Join-Path $PSScriptRoot 'dist\windows' } else { '' }
foreach ($n in $names) {
    $target = Join-Path $dest $n
    if ($local -and (Test-Path (Join-Path $local $n))) {
        Copy-Item (Join-Path $local $n) $target -Force
    } else {
        Invoke-WebRequest "$raw/dist/windows/$n" -OutFile $target -UseBasicParsing
    }
    Write-Host "установлен $n"
}
Write-Host "Готово: $dest"
Write-Host 'Открой FL Studio, Options > Manage plugins > Find installed plugins. В проекте плагины вставляй заново.'
Start-Sleep -Seconds 2
