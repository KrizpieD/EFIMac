#!/bin/bash
set -e
LLVM="/c/Program Files/LLVM/bin"
export PATH="$LLVM:$PATH"
echo "CC=$(which clang)"
$LLVM/clang -target x86_64-pc-win32-coff -mno-red-zone -ffreestanding -fshort-wchar -fno-stack-protector -fno-strict-aliasing -funsigned-char -fno-math-errno -O2 -I third_party/gnu-efi/inc -I src -Wall -Werror -c src/cpu/interpreter.c -o build/src/cpu/interpreter.obj
echo "compiled interpreter.obj"
$LLVM/lld-link /subsystem:EFI_APPLICATION /nodefaultlib /entry:efi_main /dll /out:build/EFI-Mac-Emulator.efi build/src/cpu/interpreter.obj build/src/cpu/m68k.obj build/src/cpu/translation_impl.obj build/src/cpu/emul_op.obj build/gnuefi/*.obj
echo "linked OK"
ls -la build/EFI-Mac-Emulator.efi
