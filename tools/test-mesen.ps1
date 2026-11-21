[CmdletBinding()]
param(
    [string]$MesenPath = '',
    [string]$Cc65Bin = '',
    [ValidateRange(1, 3600)]
    [int]$TimeoutSeconds = 120,
    [switch]$UseBuiltRoms,
    [switch]$AreaTrial
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

$logDir = Join-Path $root 'build/mesen-smoke'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Invoke-MesenSuite($SuitePath, $RomPath) {
    $romName = [IO.Path]::GetFileName($RomPath)
    $suiteName = [IO.Path]::GetFileName($SuitePath)
    $logBase = Join-Path $logDir "$([IO.Path]::GetFileNameWithoutExtension($romName))-$([IO.Path]::GetFileNameWithoutExtension($suiteName))"
    $arguments = "--doNotSaveSettings --testRunner --timeout=$TimeoutSeconds `"$SuitePath`" `"$RomPath`""
    Write-Host "RUN  $romName / $suiteName"
    $process = Start-Process -FilePath $MesenPath -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput "$logBase.log" -RedirectStandardError "$logBase-error.log"
    return $process.ExitCode
}

$buildScript = Join-Path $root 'build.ps1'
if ($AreaTrial) {
    if ($UseBuiltRoms) { throw '-AreaTrial generates its own fixture and cannot use -UseBuiltRoms.' }
    $fixture = Join-Path $root 'build/area-trial-fixture'
    & python (Join-Path $PSScriptRoot 'make-area-trial.py') $fixture
    if ($LASTEXITCODE -ne 0) { throw "Area trial fixture generation failed with code $LASTEXITCODE" }
    $courses = foreach ($course in 'stage.json', 'steps.json', 'finish.json') {
        Join-Path $fixture $course
    }
    $capacityCourses = $courses + $courses[1] + (Join-Path $fixture 'hazard.json') + (@($courses[2]) * 6)
    $rom = (& $buildScript -Hardware -Cc65Bin $Cc65Bin `
        -AreaManifest (Join-Path $fixture 'areas.json') -AreaTrial $capacityCourses)[0]
    foreach ($suite in 'area-trial-selector.lua', 'area-trial-hazard.lua') {
        $code = Invoke-MesenSuite (Join-Path $PSScriptRoot "mesen/$suite") $rom
        if ($code -ne 0) {
            Write-Host "FAIL $suite (Mesen exit $code). Logs: $logDir"
            exit 1
        }
        Write-Host "PASS $suite. Logs: $logDir"
    }

    $rom = (& $buildScript -Hardware -Cc65Bin $Cc65Bin `
        -AreaManifest (Join-Path $fixture 'areas.json') -AreaTrial $courses)[0]
    foreach ($suite in 'area-trial.lua', 'area-trial-steps.lua') {
        $code = Invoke-MesenSuite (Join-Path $PSScriptRoot "mesen/$suite") $rom
        if ($code -ne 0) {
            Write-Host "FAIL $suite (Mesen exit $code). Logs: $logDir"
            exit 1
        }
        Write-Host "PASS $suite. Logs: $logDir"
    }
    exit 0
}

$suiteDir = Join-Path $PSScriptRoot 'mesen'
$suites = @(Get-ChildItem -LiteralPath $suiteDir -Filter '*-smoke.lua' -File | Sort-Object Name)
if (!$suites.Count) {
    throw "No Mesen smoke tests found in $suiteDir."
}

$rom32 = Join-Path $root 'build/vrc7a-test.nes'
$rom512 = Join-Path $root 'build/vrc7a-test-512k.nes'
if (!$UseBuiltRoms) {
    & $buildScript -Cc65Bin $Cc65Bin | Out-Null
    & $buildScript -Hardware -Cc65Bin $Cc65Bin | Out-Null
}
foreach ($rom in @($rom32, $rom512)) {
    if (!(Test-Path -LiteralPath $rom -PathType Leaf)) {
        throw "ROM image not found: $rom"
    }
}
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
        $code = Invoke-MesenSuite $suite.FullName $rom
        if ($code -eq 0) {
            Write-Host "PASS $testName"
        } else {
            $failure = "$testName (Mesen exit $code)"
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
