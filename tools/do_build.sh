#!/bin/bash
set -euo pipefail
PROJECT="/mnt/c/Users/clayc/Desktop/New folder (2)/EFIMac"
export PATH="$PROJECT/tools:/mnt/c/Program Files/LLVM/bin:/mnt/c/Program Files (x86)/LLVM/bin:$PATH"
cd "$PROJECT"
echo "CC=$(command -v clang)"
make -j8 2>&1
echo "Built: build/EFI-Mac-Emulator.efi"
