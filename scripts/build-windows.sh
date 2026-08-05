#!/usr/bin/env bash
# Build EFI-Mac-Emulator on Windows with the chocolatey LLVM toolchain.
# Run from git-bash. Usage: bash scripts/build-windows.sh
set -euo pipefail

LLVM_BIN="/c/Program Files/LLVM/bin"
export PATH="$LLVM_BIN:$PATH"

cd "$(dirname "$0")/.." || exit 1

make -j8
make check

echo "Built: build/EFI-Mac-Emulator.efi"
