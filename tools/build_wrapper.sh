#!/bin/bash
export PATH="/c/Program Files/LLVM/bin:$PATH"
cd "$(dirname "$0")/.."
bash scripts/build-windows.sh
