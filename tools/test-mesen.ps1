[CmdletBinding()]
param(
    [string]$MesenPath = '',
    [string]$Cc65Bin = '',
    [ValidateRange(1, 3600)]
    [int]$TimeoutSeconds = 120,
    [switch]$UseBuiltRoms,
    [switch]$AreaTrial,
    [switch]$PlatformCourse,
    [switch]$ScrollCourse,
    [switch]$CourseSequence,
    [switch]$RevgStage1
)

$ErrorActionPreference = 'Stop'
if (@($AreaTrial, $PlatformCourse, $ScrollCourse, $CourseSequence, $RevgStage1).Where({ $_ }).Count -gt 1) {
    throw 'Choose one of -AreaTrial, -PlatformCourse, -ScrollCourse, -CourseSequence, or -RevgStage1.'
}
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
if ($RevgStage1) {
    $rom = Join-Path $root 'build/vrc7a-test-512k-revg-stage1.nes'
    if (!$UseBuiltRoms) {
        $rom = (& $buildScript -Hardware -RevgStage1 -Cc65Bin $Cc65Bin)[0]
    }
    if (!(Test-Path -LiteralPath $rom -PathType Leaf)) { throw "ROM not found: $rom." }
    $exitCode = Invoke-MesenSuite (Join-Path $PSScriptRoot 'mesen/revg-stage1.lua') $rom
    if ($exitCode -ne 0) { throw "revg-stage1 failed with code $exitCode. See $logDir." }
    Write-Host 'revg-stage1 passed.'
    return
}
if ($PlatformCourse -or $ScrollCourse -or $CourseSequence) {
    $courseName = if ($PlatformCourse) { 'platform-course' } elseif ($ScrollCourse) { 'scroll-course' } else { 'course-sequence' }
    $rom = Join-Path $root "build/vrc7a-test-512k-$courseName.nes"
    if (!$UseBuiltRoms) {
        $courseOptions = @{ PlatformCourse = $PlatformCourse; ScrollCourse = $ScrollCourse; CourseSequence = $CourseSequence }
        $rom = (& $buildScript -Hardware @courseOptions -Cc65Bin $Cc65Bin)[0]
    }
    if (!(Test-Path -LiteralPath $rom -PathType Leaf)) { throw "ROM not found: $rom." }
    $suiteName = if ($ScrollCourse) { 'scroll-course' } else { 'platform-course' }
    $suite = Join-Path $PSScriptRoot "mesen/$suiteName.lua"
    $exitCode = Invoke-MesenSuite $suite $rom
    if ($exitCode -ne 0) { throw "$courseName failed with code $exitCode. See $logDir." }
    Write-Host "$courseName passed."
    return
}
if ($AreaTrial) {
    if ($UseBuiltRoms) { throw '-AreaTrial generates its own fixture and cannot use -UseBuiltRoms.' }
    $fixture = Join-Path $root 'build/area-trial-fixture'
    & python (Join-Path $PSScriptRoot 'make-area-trial.py') $fixture
    if ($LASTEXITCODE -ne 0) { throw "Area trial fixture generation failed with code $LASTEXITCODE" }
    $courses = foreach ($course in 'stage.json', 'steps.json', 'finish.json') {
        Join-Path $fixture $course
    }
    $capacityCourses = $courses + $courses[1] + @(
        Join-Path $fixture 'hazard.json'
        Join-Path $fixture 'oneway.json'
        Join-Path $fixture 'ladder.json'
        Join-Path $fixture 'markers.json'
        Join-Path $fixture 'pit.json'
        Join-Path $fixture 'platform.json'
    ) + $courses[2]
    $batches = @(
        @{
            Courses = $capacityCourses
            Suites = 'area-trial-selector.lua', 'area-trial-hazard.lua', 'area-trial-oneway.lua',
                'area-trial-ladder.lua', 'area-trial-markers.lua', 'area-trial-pit.lua', 'area-trial-platform.lua'
        }
        @{
            Courses = $courses
            Suites = 'area-trial.lua', 'area-trial-steps.lua'
        }
        @{
            Courses = @(
                Join-Path $fixture 'platform-scroll.json'
                Join-Path $fixture 'platform-lift.json'
                Join-Path $fixture 'platform-transfer.json'
            )
            Suites = 'area-trial-platform-scroll.lua', 'area-trial-platform-lift.lua',
                'area-trial-platform-transfer.lua'
        }
    )
    foreach ($batch in $batches) {
        $rom = (& $buildScript -Hardware -Cc65Bin $Cc65Bin `
            -AreaManifest (Join-Path $fixture 'areas.json') -AreaTrial $batch.Courses)[0]
        foreach ($suite in $batch.Suites) {
            $code = Invoke-MesenSuite (Join-Path $PSScriptRoot "mesen/$suite") $rom
            if ($code -ne 0) {
                Write-Host "FAIL $suite (Mesen exit $code). Logs: $logDir"
                exit 1
            }
            Write-Host "PASS $suite. Logs: $logDir"
        }
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
