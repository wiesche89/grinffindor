param(
    [Parameter(Mandatory = $true)]
    [string]$WasmFile
)

$wasmPath = (Resolve-Path $WasmFile -ErrorAction SilentlyContinue)
if (-not $wasmPath) {
    Write-Host "[wasm-opt] Skipped: wasm file not found: $WasmFile"
    exit 0
}

$wasmOpt = $env:WASM_OPT
if (-not $wasmOpt) {
    $command = Get-Command wasm-opt -ErrorAction SilentlyContinue
    if ($command) {
        $wasmOpt = $command.Source
    }
}

if (-not $wasmOpt) {
    Write-Host "[wasm-opt] Skipped: wasm-opt not found in PATH and WASM_OPT is not set."
    exit 0
}

Write-Host "[wasm-opt] Optimizing $($wasmPath.Path)"
& $wasmOpt -Oz --enable-bulk-memory --enable-nontrapping-float-to-int --strip-debug --strip-dwarf --vacuum --dae --dce -o $wasmPath.Path $wasmPath.Path
if ($LASTEXITCODE -ne 0) {
    Write-Error "[wasm-opt] Optimization failed."
    exit $LASTEXITCODE
}

Write-Host "[wasm-opt] Done"
