param(
    [string]$Cc65Bin = '',
    [switch]$Hardware,
    [string]$AreaManifest = '',
    [string[]]$AreaTrial = @(),
    [switch]$PlatformCourse,
    [switch]$ScrollCourse,
    [switch]$CourseSequence,
    [switch]$Prototype,
    [switch]$RefreshRevgMap
)
$ErrorActionPreference = 'Stop'
if ($AreaManifest -and !$Hardware) {
    throw '-AreaManifest requires -Hardware.'
}
if ($AreaTrial -and !$AreaManifest) {
    throw '-AreaTrial requires -AreaManifest.'
}
if (@($PlatformCourse, $ScrollCourse, $CourseSequence, $Prototype).Where({ $_ }).Count -gt 1) {
    throw 'Choose one of -PlatformCourse, -ScrollCourse, -CourseSequence, or -Prototype.'
}
if ($Prototype -and !$Hardware) {
    throw '-Prototype requires -Hardware.'
}
if ($Prototype -and $AreaManifest) {
    throw '-Prototype cannot be combined with -AreaManifest.'
}
$courseName = if ($PlatformCourse) { 'platform-course' } elseif ($ScrollCourse) { 'scroll-course' } elseif ($CourseSequence) { 'course-sequence' }
if ($courseName -and !$Hardware) {
    throw 'Game-mode courses require -Hardware.'
}

if (!$Cc65Bin -and !(Get-Command ca65 -ErrorAction SilentlyContinue)) {
    $Cc65Bin = Join-Path $env:USERPROFILE 'tools/cc65/bin'
}
$assembler = 'ca65'
$linker = 'ld65'
if ($Cc65Bin) {
    $assembler = Join-Path $Cc65Bin 'ca65.exe'
    $linker = Join-Path $Cc65Bin 'ld65.exe'
}

$sourceDir = Join-Path $PSScriptRoot 'src'
$buildDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$basename = 'vrc7a-test'
$linkerConfig = 'vrc7a-test.cfg'
$assembleOptions = @()
$linkOptions = @()
if ($Hardware) {
    $basename = 'vrc7a-test-512k'
    $linkerConfig = 'vrc7a-512k.cfg'
    $assembleOptions = @('-D', 'PRG_ROM_UNITS=32')
}
$configPath = Join-Path $sourceDir $linkerConfig
if ($AreaManifest) {
    $areaDir = Join-Path $buildDir 'nes-areas'
    $manifestPath = (Resolve-Path -LiteralPath $AreaManifest).Path
    $areaTool = Join-Path $PSScriptRoot 'tools/place-nes-areas.py'
    $areaOptions = @()
    foreach ($trialPath in $AreaTrial) {
        $areaOptions += @('--trial', (Resolve-Path -LiteralPath $trialPath).Path)
    }
    $areaArgs = @($manifestPath, $configPath, $areaDir) + $areaOptions
    & python $areaTool @areaArgs
    if ($LASTEXITCODE -ne 0) { throw "Area placement failed with code $LASTEXITCODE" }
    $basename = 'vrc7a-test-512k-areas'
    $assembleOptions += @('-D', 'NES_AREA_ART=1', '-I', $areaDir)
    if ($AreaTrial) {
        $basename += '-trial'
        $assembleOptions += @('-D', 'NES_AREA_TRIAL=1')
    }
    $configPath = Join-Path $areaDir 'nes-areas.cfg'
}
if ($courseName) {
    $stageConverter = Join-Path $PSScriptRoot 'tools/convert-stage-map.py'
    $courseMaps = if ($CourseSequence) { 'platform-course', 'scroll-course' } else { $courseName }
    foreach ($courseMap in $courseMaps) {
        $courseJson = Join-Path $PSScriptRoot "tools/fixtures/$courseMap.json"
        $courseInclude = Join-Path $buildDir "$courseMap-map.inc"
        & python $stageConverter $courseJson $courseInclude --prefix course
        if ($LASTEXITCODE -ne 0) { throw "$courseMap conversion failed with code $LASTEXITCODE" }
    }
    $basename += "-$courseName"
    $assembleOptions += @('-D', 'ORIGINAL_COURSE=1', '-I', $buildDir)
    if ($ScrollCourse) { $assembleOptions += @('-D', 'SCROLL_COURSE=1') }
    if ($CourseSequence) { $assembleOptions += @('-D', 'COURSE_SEQUENCE=1') }
}
if ($RefreshRevgMap) {
    # Rebuild src/revg-section0-map.inc from the arcade ROM (reference/makaimurg.zip, not in
    # the repository). The converted map is committed, like the music and graphics data.
    $stageDir = Join-Path $buildDir 'revg-stages'
    & python (Join-Path $PSScriptRoot 'tools/mame/extract-revg-stages.py') --output $stageDir | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Stage extraction failed with code $LASTEXITCODE" }
    & python (Join-Path $PSScriptRoot 'tools/convert-revg-section.py') (Join-Path $sourceDir 'revg-section0-map.inc') --stages $stageDir | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Real map conversion failed with code $LASTEXITCODE" }
    # The background art (src/revg-section0-bg.inc) takes about a minute and a half: the
    # palette fit tries 40 starts.
    & python (Join-Path $PSScriptRoot 'tools/convert-revg-bg.py') (Join-Path $sourceDir 'revg-section0-bg.inc') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Background art conversion failed with code $LASTEXITCODE" }
    # The songs, from the Z80 sound ROM (docs/revision-g-sound-rom-map.md).
    & python (Join-Path $PSScriptRoot 'tools/convert-revg-music.py') (Join-Path $sourceDir 'arcade-music-data.inc') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Music conversion failed with code $LASTEXITCODE" }
    # The enemy placement and, in about three minutes, the sprites (docs/revision-g-sprites.md).
    & python (Join-Path $PSScriptRoot 'tools/convert-revg-spawns.py') (Join-Path $sourceDir 'revg-section0-spawns.inc') --stages $stageDir | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Spawn conversion failed with code $LASTEXITCODE" }
    & python (Join-Path $PSScriptRoot 'tools/convert-revg-sprites.py') (Join-Path $sourceDir 'revg-sprite-data.inc') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Sprite conversion failed with code $LASTEXITCODE" }
}
if ($Prototype) {
    # The prototype seven stages that the arcade-* Mesen smoke tests play.
    $basename += '-prototype'
} elseif ($Hardware -and !$courseName -and !$AreaManifest) {
    # The game: Stage 1 with the real Revision G section 0 layout.
    $assembleOptions += @('-D', 'REVG_STAGE1=1')
}
$object = Join-Path $buildDir "$basename.o"
$rom = Join-Path $buildDir "$basename.nes"
$labels = Join-Path $buildDir "$basename.lbl"
$linkedRom = $rom
if ($AreaTrial) { $linkedRom = "$rom.tmp" }
& $assembler @assembleOptions (Join-Path $sourceDir 'vrc7a-test.s') -o $object
if ($LASTEXITCODE -ne 0) { throw "ca65 exited with code $LASTEXITCODE" }
try {
    & $linker @linkOptions -C $configPath -Ln $labels -o $linkedRom $object
    if ($LASTEXITCODE -ne 0) { throw "ld65 exited with code $LASTEXITCODE" }
    if ($AreaTrial) {
        & python $areaTool --check-trial-rom $linkedRom --trial-count $AreaTrial.Count
        if ($LASTEXITCODE -ne 0) { throw "Area trial validation failed with code $LASTEXITCODE" }
        Move-Item -LiteralPath $linkedRom -Destination $rom -Force
    }
} finally {
    if ($AreaTrial -and (Test-Path -LiteralPath $linkedRom)) {
        Remove-Item -LiteralPath $linkedRom
    }
}

$chrUsage = @{}
foreach ($line in [IO.File]::ReadLines($labels)) {
    if ($line -match '^al ([0-9a-fA-F]+) \.CHR_(TEXT|TERRAIN|SPRITE)_BYTES$') {
        $chrUsage[$Matches[2]] = [Convert]::ToInt32($Matches[1], 16)
    }
}
if ($chrUsage.Count -ne 3) { throw 'Missing CHR-RAM usage symbols in the linker labels.' }
$backgroundBytes = $chrUsage.TEXT + $chrUsage.TERRAIN
$totalBytes = $backgroundBytes + $chrUsage.SPRITE
Write-Host "CHR-RAM: HUD/text $($chrUsage.TEXT) B, terrain $($chrUsage.TERRAIN) B, sprites $($chrUsage.SPRITE) B"
Write-Host "Background table: $backgroundBytes / 4096 B, sprite table: $($chrUsage.SPRITE) / 4096 B, total: $totalBytes / 8192 B"
if ($Hardware) {
    $headerSize = 16
    $prgSize = 512KB
    $romBytes = [IO.File]::ReadAllBytes($rom)
    if ($romBytes.Length -ne ($headerSize + $prgSize) -or $romBytes[4] -ne 32) {
        throw 'The 512 KiB cartridge image has an unexpected size or PRG declaration.'
    }
    $programmerImage = Join-Path $buildDir "$basename.prg"
    $prgBytes = [byte[]]::new($prgSize)
    [Array]::Copy($romBytes, $headerSize, $prgBytes, 0, $prgSize)
    [IO.File]::WriteAllBytes($programmerImage, $prgBytes)
    Write-Output $rom
    Write-Output $programmerImage
} else {
    Write-Output $rom
}
