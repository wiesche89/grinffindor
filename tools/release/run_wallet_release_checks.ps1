[CmdletBinding()]
param(
    [string]$RepoRoot = "",
    [string]$QtBin = "C:\Qt\6.10\6.10.1\mingw_64\bin",
    [string]$MingwBin = "C:\msys64\mingw64\bin",
    [switch]$BuildDesktopWallet,
    [switch]$BuildWasmWallet
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path (Split-Path -Parent $PSCommandPath) "..\..")).Path
}

function Invoke-ReleaseStep {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    Write-Host "==> $Name"
    & $Action
}

function Invoke-QMakeBuild {
    param(
        [string]$BuildDir,
        [string]$ProjectFile
    )

    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    Push-Location $BuildDir
    try {
        & (Join-Path $QtBin "qmake.exe") $ProjectFile
        & (Join-Path $MingwBin "mingw32-make.exe") -j4
    }
    finally {
        Pop-Location
    }
}

function Invoke-Exe {
    param(
        [string]$BuildDir,
        [string]$ExeRelativePath
    )

    Push-Location $BuildDir
    try {
        $env:PATH = "$QtBin;$MingwBin;$env:PATH"
        & $ExeRelativePath
    }
    finally {
        Pop-Location
    }
}

$walletRegressionBuild = Join-Path $RepoRoot "build\wallet_regression_verify"
$slatepackVerifyBuild = Join-Path $RepoRoot "build\tools_slatepack_verify"
$desktopBuild = Join-Path $RepoRoot "build\Desktop_Qt_6_10_1_MinGW_64_bit-Release"
$wasmBuild = Join-Path $RepoRoot "build\WebAssembly_Qt_6_10_1_single_threaded-Release"

Invoke-ReleaseStep "Build wallet regression verifier" {
    Invoke-QMakeBuild -BuildDir $walletRegressionBuild -ProjectFile "..\..\tools\wallet_regression_verify.pro"
}

Invoke-ReleaseStep "Run wallet regression verifier" {
    Invoke-Exe -BuildDir $walletRegressionBuild -ExeRelativePath ".\release\wallet_regression_verify.exe"
}

Invoke-ReleaseStep "Build slatepack interop verifier" {
    Invoke-QMakeBuild -BuildDir $slatepackVerifyBuild -ProjectFile "..\..\tools\slatepack_reader_verify.pro"
}

Invoke-ReleaseStep "Run slatepack interop verifier" {
    Invoke-Exe -BuildDir $slatepackVerifyBuild -ExeRelativePath ".\release\slatepack_reader_verify.exe"
}

if ($BuildDesktopWallet) {
    Invoke-ReleaseStep "Build desktop release wallet" {
        Push-Location $desktopBuild
        try {
            & (Join-Path $MingwBin "mingw32-make.exe") -j4
        }
        finally {
            Pop-Location
        }
    }
}

if ($BuildWasmWallet) {
    Invoke-ReleaseStep "Build wasm release wallet" {
        Push-Location $wasmBuild
        try {
            & (Join-Path $MingwBin "mingw32-make.exe") -j4
        }
        finally {
            Pop-Location
        }
    }

    Invoke-ReleaseStep "Check wasm release artifacts" {
        $required = @(
            (Join-Path $wasmBuild "grinffindor.html"),
            (Join-Path $wasmBuild "grinffindor.js"),
            (Join-Path $wasmBuild "grinffindor.wasm"),
            (Join-Path $wasmBuild "qtloader.js")
        )
        foreach ($path in $required) {
            if (-not (Test-Path $path)) {
                throw "Missing WASM artifact: $path"
            }
        }
    }
}

Write-Host "Release wallet checks passed."
