import struct, os

def elf32_powerpc_big(words):
    text = b"".join(struct.pack(">I", w) for w in words)
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
    ph = struct.pack(">IIIIIIII", 1, 0, 0, 0x10000, 0x10000, 0x1000, 0x1000 + len(text), 0)
    sh0 = bytes(40)
    sh1 = struct.pack(">IIIIIIIIII", 1, 0, 1, 0x10000, 0, 0x10000, len(text), 0, 0, 2)
    sh2 = struct.pack(">IIIIIIIIII", 1, 0, 2, 0x10000, 0x10000, len(text), 0, 0, 0, 0)
    shstr = b"\x00.text\x00.shstrtab\x00"
    sh3 = struct.pack(">IIIIIIIIII", 3, 0, 0, 0, 0, 0, len(shstr), 0, 0, 3)
    return hdr + ph + text + sh0 + sh1 + sh2 + sh3 + shstr

words = [0x7D1040D6, 0x7D000214, 0x7C0001D6, 0x7C0001F6, 0x7C000214, 0x7C0802A6, 0x7C0003A6]
out = os.path.join(os.environ["TEMP"], "opencode", "p.o")
open(out, "wb").write(elf32_powerpc_big(words))
print("wrote", out)
