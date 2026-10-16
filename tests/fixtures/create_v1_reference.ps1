# Independent historical SaveFormatVersion 1 fixture. Never called by normal tests.
# Explicit wire fields, no current C++ encoder dependency. Output stays beside this script.
$ErrorActionPreference = 'Stop'
$faultlineStream = [System.IO.MemoryStream]::new()
$script:faultlineWriter = [System.IO.BinaryWriter]::new($faultlineStream)
function U8([byte]$v) { $script:faultlineWriter.Write($v) }
function U16([UInt16]$v) { $script:faultlineWriter.Write($v) }
function U32([UInt32]$v) { $script:faultlineWriter.Write($v) }
function U64([UInt64]$v) { $script:faultlineWriter.Write($v) }
function I64([Int64]$v) { $script:faultlineWriter.Write($v) }
function F32([single]$v) { $script:faultlineWriter.Write($v) }
function Text([string]$v) {
    $faultlineUtf8 = [System.Text.Encoding]::UTF8.GetBytes($v)
    U32 $faultlineUtf8.Length
    $script:faultlineWriter.Write($faultlineUtf8)
}
function Section([UInt32]$tag, [scriptblock]$body) {
    $faultlineOuter = $script:faultlineWriter
    $faultlinePayload = [System.IO.MemoryStream]::new()
    $script:faultlineWriter = [System.IO.BinaryWriter]::new($faultlinePayload)
    try {
        & $body
        $script:faultlineWriter.Flush()
        $faultlineOuter.Write($tag)
        $faultlineOuter.Write([UInt32]$faultlinePayload.Length)
        $faultlineOuter.Write($faultlinePayload.ToArray())
    } finally {
        $script:faultlineWriter.Dispose()
        $script:faultlineWriter = $faultlineOuter
        $faultlinePayload.Dispose()
    }
}
try {
    $script:faultlineWriter.Write([System.Text.Encoding]::ASCII.GetBytes("FAULTLINE_SAVE" + [char]0 + [char]0))
    U32 1; U32 0; U32 1; U32 0; U32 3; U64 12345; U32 10
    Section 1 {
        U64 64; U64 144000; F32 128; U32 7
        for ($faultlineDomain=0; $faultlineDomain -lt 7; ++$faultlineDomain) { U64 1; U64 2; U64 3; U64 4 }
    }
    Section 2 { U8 0 } # No generated base; semantic stores below remain fully valid.
    Section 3 { U64 4; U32 0; U32 0; U32 0; U32 0 }
    Section 4 { U64 2; U32 1; U64 1; U64 0 }
    Section 5 { U64 2; U32 1; U64 1; U64 1; U64 1; U32 3 }
    Section 6 {
        U64 2; U32 1; U64 1
        U8 0; U64 1; U64 1; Text 'Legacy'; U8 1
        U16 5000; U16 4000; U16 3000; U16 6000
        I64 123456; U32 20; U16 7000
    }
    Section 7 {
        U64 2; U32 1; U64 1
        U8 0; U64 1; U64 1; U64 1; U64 0; U64 0
        U32 0; U32 0; F32 0; F32 0; F32 0; F32 75; F32 100
        Text 'Legacy Boss'; U8 1; U64 99
        U16 3; U8 5; U8 65; U16 8000
        for ($faultlineTrait=0; $faultlineTrait -lt 11; ++$faultlineTrait) { U16 5000 }
        U8 1
    }
    Section 8 { U64 1; U32 0 }
    Section 9 { U64 2; U32 1; U64 1; U8 0; U64 1 }
    Section 10 { U32 0; U32 0 }
    $script:faultlineWriter.Flush()
    $faultlineFixture = Join-Path $PSScriptRoot 'save_v1_reference.flsave'
    [System.IO.File]::WriteAllBytes($faultlineFixture, $faultlineStream.ToArray())
    Get-FileHash -Algorithm SHA256 -LiteralPath $faultlineFixture
} finally {
    $script:faultlineWriter.Dispose()
    $faultlineStream.Dispose()
}
