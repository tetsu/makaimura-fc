[CmdletBinding()]
param(
    [string]$MesenPath = '',
    [string]$Cc65Bin = '',
    [ValidateRange(1, 3600)]
    [int]$TimeoutSeconds = 120,
    [switch]$UseBuiltRoms
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$MesenPath) {
    $MesenPath = Join-Path $root 'build/mesen/Mesen.exe'
}
if (!(Test-Path -LiteralPath $MesenPath -PathType Leaf)) {
    throw "Mesen executable not found: $MesenPath. Use -MesenPath to specify its location."
}
$MesenPath = (Resolve-Path -LiteralPath $MesenPath).Path

$suiteDir = Join-Path $PSScriptRoot 'mesen'
$suites = @(Get-ChildItem -LiteralPath $suiteDir -Filter '*-smoke.lua' -File | Sort-Object Name)
if (!$suites.Count) {
    throw "No Mesen smoke tests found in $suiteDir."
}

$rom32 = Join-Path $root 'build/vrc7a-test.nes'
$rom512 = Join-Path $root 'build/vrc7a-test-512k.nes'
if (!$UseBuiltRoms) {
    $buildScript = Join-Path $root 'build.ps1'
    & $buildScript -Cc65Bin $Cc65Bin | Out-Null
    & $buildScript -Hardware -Cc65Bin $Cc65Bin | Out-Null
}
foreach ($rom in @($rom32, $rom512)) {
    if (!(Test-Path -LiteralPath $rom -PathType Leaf)) {
        throw "ROM image not found: $rom"
    }
}
$logDir = Join-Path $root 'build/mesen-smoke'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$failures = @()
$testCount = 0
foreach ($rom in @($rom32, $rom512)) {
    $romName = [IO.Path]::GetFileName($rom)
    $is512k = ($rom -eq $rom512)
    foreach ($suite in $suites) {
        $isArcadeSuite = $suite.Name -like 'arcade-*'
        if ($is512k -and !$isArcadeSuite -and $suite.Name -ne 'campaign-smoke.lua') { continue }
        if (!$is512k -and $isArcadeSuite) { continue }
        $testCount++
        $testName = "$romName / $($suite.Name)"
        $logBase = Join-Path $logDir "$([IO.Path]::GetFileNameWithoutExtension($rom))-$($suite.BaseName)"
        $arguments = "--doNotSaveSettings --testRunner --timeout=$TimeoutSeconds `"$($suite.FullName)`" `"$rom`""
        Write-Host "RUN  $testName"
        $process = Start-Process -FilePath $MesenPath -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput "$logBase.log" -RedirectStandardError "$logBase-error.log"
        if ($process.ExitCode -eq 0) {
            Write-Host "PASS $testName"
        } else {
            $failure = "$testName (Mesen exit $($process.ExitCode))"
            $failures += $failure
            Write-Host "FAIL $failure"
        }
    }
}

Write-Host "$($testCount - $failures.Count)/$testCount passed. Logs: $logDir"
if ($failures.Count) {
    $failures | ForEach-Object { Write-Host "FAIL $_" }
    exit 1
}
exit 0
