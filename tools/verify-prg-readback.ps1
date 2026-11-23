[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Readback,
    [string]$Prg
)

$ErrorActionPreference = 'Stop'
if (!$Prg) {
    $Prg = Join-Path $PSScriptRoot '../build/vrc7a-test-512k-prototype.prg'
}
$expectedFile = Get-Item -LiteralPath $Prg
$readbackFile = Get-Item -LiteralPath $Readback
if ($expectedFile.FullName -eq $readbackFile.FullName) {
    throw 'Select the programmer readback as a separate file from the source PRG.'
}

$expectedBytes = [IO.File]::ReadAllBytes($expectedFile.FullName)
$readbackBytes = [IO.File]::ReadAllBytes($readbackFile.FullName)
$prgSize = 512KB
if ($expectedBytes.Length -ne $prgSize) {
    throw "Source PRG is $($expectedBytes.Length) bytes. Expected a headerless $prgSize-byte image."
}
if ($readbackBytes.Length -ne $prgSize) {
    throw "Readback is $($readbackBytes.Length) bytes. Expected a headerless $prgSize-byte image."
}

for ($offset = 0; $offset -lt $prgSize; $offset++) {
    if ($expectedBytes[$offset] -ne $readbackBytes[$offset]) {
        $bankSize = 8KB
        $bank = [Math]::Floor($offset / $bankSize)
        $bankOffset = $offset % $bankSize
        throw ('Mismatch at PRG offset 0x{0:X5} (bank {1}, offset 0x{2:X4}). Expected 0x{3:X2}, read 0x{4:X2}.' -f
            $offset, $bank, $bankOffset, $expectedBytes[$offset], $readbackBytes[$offset])
    }
}

$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $hash = [BitConverter]::ToString($sha256.ComputeHash($expectedBytes)).Replace('-', '')
} finally {
    $sha256.Dispose()
}
Write-Output "Readback matches all $prgSize bytes. SHA-256: $hash"
