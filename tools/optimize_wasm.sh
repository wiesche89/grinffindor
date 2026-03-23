#!/bin/sh
set -eu

wasm_file="${1:-}"

if [ -z "$wasm_file" ] || [ ! -f "$wasm_file" ]; then
  echo "[wasm-opt] Skipped: wasm file not found: $wasm_file"
  exit 0
fi

wasm_opt="${WASM_OPT:-}"
if [ -z "$wasm_opt" ]; then
  wasm_opt="$(command -v wasm-opt || true)"
fi

if [ -z "$wasm_opt" ]; then
  echo "[wasm-opt] Skipped: wasm-opt not found in PATH and WASM_OPT is not set."
  exit 0
fi

echo "[wasm-opt] Optimizing $wasm_file"
"$wasm_opt" -Oz --enable-bulk-memory --enable-nontrapping-float-to-int --strip-debug --strip-dwarf --vacuum --dae --dce -o "$wasm_file" "$wasm_file"
echo "[wasm-opt] Done"
