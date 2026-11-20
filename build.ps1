param([string]$Cc65Bin = '')
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
$object = Join-Path $buildDir 'vrc7a-test.o'
$rom = Join-Path $buildDir 'vrc7a-test.nes'
& $assembler (Join-Path $sourceDir 'vrc7a-test.s') -o $object
if ($LASTEXITCODE -ne 0) { throw "ca65 exited with code $LASTEXITCODE" }
& $linker -C (Join-Path $sourceDir 'vrc7a-test.cfg') -o $rom $object
if ($LASTEXITCODE -ne 0) { throw "ld65 exited with code $LASTEXITCODE" }
Write-Output $rom
