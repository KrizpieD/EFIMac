# EFI-Mac-Emulator build
#
# Cross-builds a UEFI x86_64 application (EFI-Mac-Emulator.efi) using a
# clang/LLVM toolchain targeting PE/COFF, linked with lld-link. GNU-EFI
# provides the UEFI headers and a small runtime library.
#
# Requirements:
#   macOS:  brew install llvm lld  (llvm-objdump comes with llvm)
#   Windows: chocolatey install llvm (or add LLVM\bin to PATH), GNU make,
#            git-bash on PATH so `make` finds /bin/sh
#   Linux:  apt install clang lld
#
# Host detection: on macOS the Homebrew prefix is used if available; other
# hosts resolve clang/lld-link from PATH. Override via
# make CC=/path/to/clang LLD=/path/to/lld-link

SHELL   := /bin/sh

# --- Toolchain discovery (override via make CC=/path/to/clang LLD=/path/to/lld-link) ---
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LLVM_PREFIX := $(shell brew --prefix llvm 2>/dev/null || echo /opt/homebrew/opt/llvm)
LLD_PREFIX  := $(shell brew --prefix lld  2>/dev/null || echo /opt/homebrew/opt/lld)
CC      = $(LLVM_PREFIX)/bin/clang
LLD     ?= $(LLD_PREFIX)/bin/lld-link
OBJDUMP = $(LLVM_PREFIX)/bin/llvm-objdump
else
# Plain "?=" would let a host environment CC (e.g. git-bash's CC=cc) leak in;
# use ":=" so the PATH-resolved clang wins unless overridden on the command line.
CC      := clang
LLD     := lld-link
OBJDUMP := llvm-objdump
endif

# --- Layout ---
GNUEFI_DIR  := third_party/gnu-efi
GNUEFI_URL  := https://git.code.sf.net/p/gnu-efi/code
BUILD_DIR   := build
OBJ_GNUEFI  := $(BUILD_DIR)/gnuefi
OBJ_SRC     := $(BUILD_DIR)/src
TARGET      := $(BUILD_DIR)/EFI-Mac-Emulator.efi

# --- Flags ---
ARCH       := -target x86_64-pc-win32-coff
COMMON     := -mno-red-zone -ffreestanding -fshort-wchar \
              -fno-stack-protector -fno-strict-aliasing -funsigned-char \
              -fno-math-errno
CFLAGS     := $(ARCH) $(COMMON) -O2 -I $(GNUEFI_DIR)/inc -I src

# GNU-EFI runtime library sources (mirrors gnu-efi lib/Makefile for x86_64,
# minus entry.c/ctors.o (require ELF crt startup) and the .S files).
GNUEFI_SRCS := \
	boxdraw smbios console crc data debug dpath \
	error event exit guid hand hw init lock \
	misc pause print sread str cmdline \
	runtime/rtlock runtime/efirtlib runtime/rtstr runtime/vm runtime/rtdata \
	x86_64/initplat x86_64/math x86_64/callwrap

# Application sources
APP_SRCS := \
	src/main.c \
	src/ui/ui_impl.c \
	src/cpu/interpreter.c \
	src/cpu/translation_impl.c \
	src/memory/manager_impl.c \
	src/hardware/abstraction_impl.c \
	src/boot/bootloader_impl.c \
	src/fs/hfs.c \
	src/utils/debug_impl.c \
	src/platform/uefi_interface_impl.c

GNUEFI_OBJS := $(patsubst %,$(OBJ_GNUEFI)/%.obj,$(GNUEFI_SRCS))
APP_OBJS    := $(patsubst src/%,$(OBJ_SRC)/%,$(APP_SRCS:.c=.obj))

GNUEFI_MARK := $(GNUEFI_DIR)/inc/efi.h

.PHONY: all gnuefi check clean

all: $(TARGET)

# --- Acquire GNU-EFI if not present ---
$(GNUEFI_MARK):
	@test -d $(GNUEFI_DIR) || git clone --depth 1 $(GNUEFI_URL) $(GNUEFI_DIR)

# --- GNU-EFI runtime library ---
$(GNUEFI_OBJS): $(GNUEFI_MARK)
$(OBJ_GNUEFI)/%.obj: $(GNUEFI_DIR)/lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unknown-pragmas -c $< -o $@

# --- Application objects ---
$(OBJ_SRC)/%.obj: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -Werror -c $< -o $@

# --- Link ---
$(TARGET): $(GNUEFI_OBJS) $(APP_OBJS)
	$(LLD) /subsystem:EFI_APPLICATION /nodefaultlib /entry:efi_main /dll \
	    /out:$@ $(APP_OBJS) $(GNUEFI_OBJS)

# --- Verification ---
check: $(TARGET)
	$(OBJDUMP) -x $(TARGET) | grep -E "Subsystem|Base Relocation"

clean:
	rm -rf $(BUILD_DIR)
