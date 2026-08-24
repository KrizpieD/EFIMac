import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
ci = 0x30D000
names = {0x28:"ROMImageBaseOffset",0x2C:"ROMImageSize",0x34:"Mac68KROMOffset",0x38:"Mac68KROMSize",
0x3C:"ExceptionTableOffset",0x40:"ExceptionTableSize",0x44:"HWInitCodeOffset",0x48:"HWInitCodeSize",
0x4C:"KernelCodeOffset",0x50:"KernelCodeSize",0x54:"EmulatorCodeOffset",0x58:"EmulatorCodeSize",
0x5C:"OpcodeTableOffset",0x60:"OpcodeTableSize",0x74:"BootVersionOffset",0x78:"ECBOffset",
0x7C:"IplValueOffset",0x80:"EmulatorEntryOffset",0x84:"KernelTrapTableOffset"}
for off in sorted(names):
    w = struct.unpack(">I", rom[ci+off:ci+off+4])[0]
    print(f"+{off:#04x} {names[off]:24s} = 0x{w:08X}")
