#!/usr/bin/env python3
"""PEF relocation + loader-section decoder.

Decodes the loader section of a PEF container and executes the relocation
bytecode for each section, reporting every fixup.  Bit layouts follow Apple's
PEFBinaryFormat.h (PEFRelocField(chunk, offset, length)).
"""

import struct


def u16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def s32(b, o):
    return struct.unpack_from(">i", b, o)[0]


def field(chunk, off, length):
    return (chunk >> (16 - (off + length))) & ((1 << length) - 1)


class Loader:
    def __init__(self, data, base):
        self.b = data
        self.base = base
        self.sections = []
        n = u16(data, base + 32)
        for i in range(n):
            o = base + 40 + 28 * i
            self.sections.append(dict(
                nameOff=s32(data, o),
                defaultAddress=u32(data, o + 4),
                totalLength=u32(data, o + 8),
                unpackedLength=u32(data, o + 12),
                containerLength=u32(data, o + 16),
                containerOffset=u32(data, o + 20),
                kind=data[o + 24],
                share=data[o + 25],
                align=data[o + 26],
            ))
        self.loaderSec = next(i for i, s in enumerate(self.sections)
                              if s["kind"] == 4)
        self.L = base + self.sections[self.loaderSec]["containerOffset"]

    def parse(self):
        b, L = self.b, self.L
        self.mainSection = s32(b, L)
        self.mainOffset = u32(b, L + 4)
        self.initSection = s32(b, L + 8)
        self.initOffset = u32(b, L + 12)
        self.termSection = s32(b, L + 16)
        self.termOffset = u32(b, L + 20)
        self.libCount = u32(b, L + 24)
        self.symCount = u32(b, L + 28)
        self.relocSectionCount = u32(b, L + 32)
        self.relocInstrOffset = u32(b, L + 36)
        self.loaderStringsOffset = u32(b, L + 40)
        self.exportHashOffset = u32(b, L + 44)
        self.hashPower = u32(b, L + 48)
        self.exportCount = u32(b, L + 52)
        self.strings = L + self.loaderStringsOffset
        self.libs = []
        p = L + 56
        for _ in range(self.libCount):
            self.libs.append(dict(
                nameOffset=u32(b, p),
                oldImpVersion=u32(b, p + 4),
                currentVersion=u32(b, p + 8),
                symbolCount=u32(b, p + 12),
                firstSymbol=u32(b, p + 16),
                options=b[p + 20],
            ))
            p += 24
        self.symbols = []
        for _ in range(self.symCount):
            cn = u32(b, p)
            self.symbols.append(dict(cls=cn >> 24, nameOffset=cn & 0xFFFFFF))
            p += 4
        self.relocHeaders = []
        for _ in range(self.relocSectionCount):
            self.relocHeaders.append(dict(
                sectionIndex=u16(b, p),
                reserved=u16(b, p + 2),
                relocCount=u32(b, p + 4),
                firstRelocOffset=u32(b, p + 8),
            ))
            p += 12

    def name(self, off):
        o = self.strings + off
        e = self.b.find(b"\0", o)
        return self.b[o:e].decode("latin1")

    def lib_name(self, libIndex):
        for lb in self.libs:
            if libIndex < lb["symbolCount"] + lb["firstSymbol"]:
                if libIndex >= lb["firstSymbol"]:
                    return lb
        return None


def run_relocs(ld, hdr, trace):
    b = ld.b
    p = ld.L + ld.relocInstrOffset + hdr["firstRelocOffset"]
    sec = ld.sections[hdr["sectionIndex"]]
    relocAddr = sec["defaultAddress"]
    importIndex = 0
    sectionC = 0
    sectionD = 0
    endBlock = ld.L + ld.relocInstrOffset + hdr["firstRelocOffset"] + hdr["relocCount"] * 2
    events = []
    while p < endBlock:
        chunk = u16(b, p)
        op = chunk >> 9
        if op < 0x20:  # RelocBySectDWithSkip
            skip = field(chunk, 2, 8)
            count = field(chunk, 10, 6)
            relocAddr += skip * 4
            events.append(("BySectDWithSkip", relocAddr, count))
            relocAddr += count * 4
            p += 2
        elif 0x20 <= op <= 0x25:
            sub = field(chunk, 3, 4)
            run = field(chunk, 7, 9) + 1
            events.append(("Run", sub, relocAddr, run))
            if sub == 0:
                relocAddr += run * 4
            elif sub == 1:
                relocAddr += run * 4
            elif sub == 2:
                relocAddr += run * 12
            elif sub == 3:
                relocAddr += run * 8
            elif sub == 4:
                relocAddr += run * 8
            elif sub == 5:
                events.append(("ImportRun", relocAddr, run, importIndex))
                importIndex += run
                relocAddr += run * 4
            p += 2
        elif 0x30 <= op <= 0x33:
            sub = field(chunk, 3, 4)
            idx = field(chunk, 7, 9)
            if sub == 0:
                events.append(("SmByImport", relocAddr, idx, ld.name(ld.symbols[idx]["nameOffset"])))
                importIndex = idx + 1
                relocAddr += 4
            elif sub == 1:
                sectionC = idx
                events.append(("SmSetSectC", idx))
            elif sub == 2:
                sectionD = idx
                events.append(("SmSetSectD", idx))
            elif sub == 3:
                events.append(("SmBySection", relocAddr, idx))
                relocAddr += 4
            p += 2
        elif 0x40 <= op <= 0x47:  # RelocIncrPosition
            off = field(chunk, 4, 12) + 1
            relocAddr += off
            events.append(("IncrPosition", relocAddr))
            p += 2
        elif 0x48 <= op <= 0x4F:  # RelocSmRepeat
            cc = field(chunk, 4, 4) + 1
            rc = field(chunk, 8, 8) + 1
            events.append(("SmRepeat", cc, rc))
            p += 2
        elif op == 0x50:  # RelocSetPosition (2 chunks)
            hi = field(chunk, 6, 10)
            off = (hi << 16) | u16(b, p + 2)
            relocAddr = sec["defaultAddress"] + off
            events.append(("SetPosition", relocAddr))
            p += 4
        elif op == 0x52:  # RelocLgByImport (2 chunks)
            hi = field(chunk, 6, 10)
            idx = (hi << 16) | u16(b, p + 2)
            events.append(("LgByImport", relocAddr, idx, ld.name(ld.symbols[idx]["nameOffset"])))
            importIndex = idx + 1
            relocAddr += 4
            p += 4
        elif op == 0x58:  # RelocLgRepeat (2 chunks)
            cc = field(chunk, 6, 4) + 1
            rc = (field(chunk, 10, 6) << 16) | u16(b, p + 2)
            events.append(("LgRepeat", cc, rc))
            p += 4
        elif op == 0x5A:  # RelocLgSetOrBySection (2 chunks)
            sub = field(chunk, 6, 4)
            idx = (field(chunk, 10, 6) << 16) | u16(b, p + 2)
            if sub == 0:
                events.append(("LgBySection", relocAddr, idx))
                relocAddr += 4
            elif sub == 1:
                sectionC = idx
                events.append(("LgSetSectC", idx))
            elif sub == 2:
                sectionD = idx
                events.append(("LgSetSectD", idx))
            p += 4
        else:
            events.append(("UNKNOWN", hex(chunk), p - ld.L))
            break
    return events


if __name__ == "__main__":
    import sys
    data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\system_data_fork.bin", "rb").read()
    base = 0x2A0
    ld = Loader(data, base)
    ld.parse()
    print("libs=%d syms=%d relocSections=%d" % (ld.libCount, ld.symCount, ld.relocSectionCount))
    for i, h in enumerate(ld.relocHeaders):
        print("reloc hdr %d: section=%d count=%d firstOff=0x%X" % (
            i, h["sectionIndex"], h["relocCount"], h["firstRelocOffset"]))
    for h in ld.relocHeaders:
        print("=== relocations for section %d ===" % h["sectionIndex"])
        ev = run_relocs(ld, h, True)
        for e in ev:
            print("   ", e)
