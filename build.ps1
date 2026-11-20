param(
    [string]$Cc65Bin = '',
    [switch]$Hardware
)
$ErrorActionPreference = 'Stop'

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
$assembleOptions = @()
$linkOptions = @()
if ($Hardware) {
    $basename = 'vrc7a-test-512k'
    $assembleOptions = @('-D', 'PRG_ROM_UNITS=32')
    # Banks 0–2 stay at the beginning. The fixed bank is the last of 64.
    $linkOptions = @('-D', 'PRG_PADDING=$78000')
}
$object = Join-Path $buildDir "$basename.o"
$rom = Join-Path $buildDir "$basename.nes"
& $assembler @assembleOptions (Join-Path $sourceDir 'vrc7a-test.s') -o $object
if ($LASTEXITCODE -ne 0) { throw "ca65 exited with code $LASTEXITCODE" }
& $linker @linkOptions -C (Join-Path $sourceDir 'vrc7a-test.cfg') -o $rom $object
if ($LASTEXITCODE -ne 0) { throw "ld65 exited with code $LASTEXITCODE" }
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
