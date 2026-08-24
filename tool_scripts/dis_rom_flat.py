import struct, sys, subprocess, os

ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin"
ROM_BASE = 0x40800000
OUT = os.path.join(os.environ["TEMP"], "opencode", "rom_slice.o")

def elf32_powerpc_big(text, vaddr):
    e_ehsize = 52
    e_phoff = 52
    e_phentsize = 32
    e_phnum = 1
    e_shoff = 52 + 32
    e_shentsize = 40
    e_shnum = 4
    e_shstrndx = 3
    ei = b"\x7fELF" + bytes([1, 2, 1, 0]) + bytes(8)
    hdr = struct.pack(">16sHHIIIIIHHHHHH", ei, 2, 20, 1, 0, e_phoff, e_shoff, 0,
                      e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx)
    ph = struct.pack(">IIIIIIII", 1, 0, 0, vaddr, vaddr, 0x1000, 0x1000 + len(text), 0)
    sh0 = bytes(40)
    sh1 = struct.pack(">IIIIIIIIII", 1, 0, 1, vaddr, 0, vaddr, len(text), 0, 0, 2)
    sh2 = struct.pack(">IIIIIIIIII", 1, 0, 2, vaddr, 0x1000, 0, 0, 0, 0, 0)
    shstr = b"\x00.text\x00.shstrtab\x00"
    sh3 = struct.pack(">IIIIIIIIII", 3, 0, 0, 0, 0, 0, len(shstr), 0, 0, 3)
    return hdr + ph + text + sh0 + sh1 + sh2 + sh3 + shstr

start = int(sys.argv[1], 0)
stop = int(sys.argv[2], 0)
data = open(ROM, "rb").read()
text = data[start - ROM_BASE:stop - ROM_BASE]
open(OUT, "wb").write(elf32_powerpc_big(text, start))
subprocess.run([r"C:\Program Files\LLVM\bin\llvm-objdump.exe", "-d", "--triple=powerpc",
                "--start-address=0x%x" % start, "--stop-address=0x%x" % stop,
                OUT])
