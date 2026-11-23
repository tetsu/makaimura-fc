[CmdletBinding()]
param(
    [string]$Cc65Bin = '',
    [switch]$Mesen
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
# The hardware and Mesen trials check the prototype seven stages (docs/vrc7a-hardware-test.md).
& (Join-Path $root 'build.ps1') -Hardware -Prototype -Cc65Bin $Cc65Bin | Out-Null

$images = @('build/vrc7a-test-512k-prototype.prg', 'build/vrc7a-test-512k-prototype.nes')
$files = $images + @(
    'docs/vrc7a-hardware-test.md'
    'docs/vrc7a-frames/start.png'
    'docs/vrc7a-frames/clear.png'
    'docs/vrc7a-frames/before-diagonal.png'
    'docs/vrc7a-frames/diagonal.png'
    'docs/vrc7a-frames/spark.png'
    'docs/vrc7a-frames/retry.png'
    'tools/verify-prg-readback.ps1'
    'tools/measure-audio.py'
    'tools/mesen/audio-record-modes.lua'
    'tools/mesen/course-clear-smoke.lua'
)
$archiveName = 'vrc7a-352402-trial.zip'
if ($Mesen) {
    & (Join-Path $root 'build.ps1') -Cc65Bin $Cc65Bin | Out-Null
    $images = @('build/vrc7a-test.nes', 'build/vrc7a-test-512k-prototype.nes')
    $files = $images + @('docs/vrc7a-mesen-trial.md', 'tools/test-mesen.ps1')
    $suiteDir = Join-Path $PSScriptRoot 'mesen'
    $files += Get-ChildItem -LiteralPath $suiteDir -Filter '*-smoke.lua' -File | Sort-Object Name |
        ForEach-Object { "tools/mesen/$($_.Name)" }
    $archiveName = 'vrc7a-mesen-trial.zip'
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$output = Join-Path $root "build/$archiveName"
$temp = Join-Path (Split-Path -Parent $output) ([Guid]::NewGuid().ToString() + '.zip')
try {
    $archive = [IO.Compression.ZipFile]::Open($temp, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $files) {
            $source = Join-Path $root $file
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $source, $file) | Out-Null
        }
        $manifest = $archive.CreateEntry('SHA256SUMS.txt')
        $writer = [IO.StreamWriter]::new($manifest.Open())
        try {
            foreach ($file in $images) {
                $hash = (Get-FileHash -LiteralPath (Join-Path $root $file) -Algorithm SHA256).Hash
                $writer.WriteLine('{0}  {1}', $hash, $file)
            }
        } finally {
            $writer.Dispose()
        }
    } finally {
        $archive.Dispose()
    }
    Move-Item -LiteralPath $temp -Destination $output -Force
} finally {
    if (Test-Path -LiteralPath $temp) {
        Remove-Item -LiteralPath $temp
    }
}
Write-Output $output
