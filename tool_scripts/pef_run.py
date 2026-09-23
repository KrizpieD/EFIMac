#!/usr/bin/env python3
"""Validate PEF section placement + relocation for one fragment.

Places the code and data sections at chosen runtime bases, expands the
pattern data, applies the section-1 relocations, binds import pointers to
synthetic TVector addresses, then reports the resulting transition vectors.
"""

import struct
import sys

sys.path.insert(0, r"C:\Users\clayc\Desktop\EFIMac\tool_scripts")
from pef_pattern import unpack_pattern
from pef_reloc import Loader, run_relocs, u32, u16


def apply_relocs(buf, events, code_base, data_base, sec_bases, import_addrs):
    relocAddr = 0
    sectionC = code_base
    sectionD = data_base
    for e in events:
        kind = e[0]
        if kind == "SetPosition":
            relocAddr = e[1]
        elif kind == "SmSetSectC" or kind == "LgSetSectC":
            sectionC = sec_bases.get(e[1], sec_bases.get(0, code_base))
        elif kind == "SmSetSectD" or kind == "LgSetSectD":
            sectionD = sec_bases.get(e[1], sec_bases.get(1, data_base))
        elif kind == "BySectDWithSkip":
            _, off, count = e
            for k in range(count):
                p = off + 4 * k
                buf[p:p + 4] = struct.pack(">I", (u32(buf, p) + sectionD) & 0xFFFFFFFF)
        elif kind == "Run":
            _, sub, off, run = e
            if sub == 0:  # BySectC
                for k in range(run):
                    p = off + 4 * k
                    buf[p:p + 4] = struct.pack(">I", (u32(buf, p) + sectionC) & 0xFFFFFFFF)
            elif sub == 1:  # BySectD
                for k in range(run):
                    p = off + 4 * k
                    buf[p:p + 4] = struct.pack(">I", (u32(buf, p) + sectionD) & 0xFFFFFFFF)
            elif sub == 2:  # TVector12
                for k in range(run):
                    p = off + 12 * k
                    buf[p:p + 4] = struct.pack(">I", (u32(buf, p) + sectionC) & 0xFFFFFFFF)
                    buf[p + 4:p + 8] = struct.pack(">I", (u32(buf, p + 4) + sectionD) & 0xFFFFFFFF)
            elif sub == 3:  # TVector8
                for k in range(run):
                    p = off + 8 * k
                    buf[p:p + 4] = struct.pack(">I", (u32(buf, p) + sectionC) & 0xFFFFFFFF)
                    buf[p + 4:p + 8] = struct.pack(">I", (u32(buf, p + 4) + sectionD) & 0xFFFFFFFF)
            elif sub == 4:  # VTable8
                for k in range(run):
                    p = off + 8 * k
                    buf[p:p + 4] = struct.pack(">I", (u32(buf, p) + sectionD) & 0xFFFFFFFF)
        elif kind == "ImportRun":
            _, off, run, start = e
            for k in range(run):
                p = off + 4 * k
                buf[p:p + 4] = struct.pack(">I", (u32(buf, p) + import_addrs[start + k]) & 0xFFFFFFFF)
        elif kind == "SmByImport":
            _, off, idx, name = e
            buf[off:off + 4] = struct.pack(">I", (u32(buf, off) + import_addrs[idx]) & 0xFFFFFFFF)
        elif kind == "LgByImport":
            _, off, idx, name = e
            buf[off:off + 4] = struct.pack(">I", (u32(buf, off) + import_addrs[idx]) & 0xFFFFFFFF)
    return buf


def main():
    try:
        sys.stdout.reconfigure(errors="replace")
    except Exception:
        pass
    data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\system_data_fork.bin", "rb").read()
    base = 0x2A0
    ld = Loader(data, base)
    ld.parse()
    sec = ld.sections
    code_base = 0x01000000
    data_base = 0x02000000
    sec_bases = {0: code_base, 1: data_base, 2: 0, 3: 0}

    code = bytearray(data[base + sec[0]["containerOffset"]:
                          base + sec[0]["containerOffset"] + sec[0]["containerLength"]])
    dat = bytearray(unpack_pattern(data, base + sec[1]["containerOffset"],
                                   sec[1]["containerLength"], sec[1]["unpackedLength"]))
    dat += b"\x00" * (sec[1]["totalLength"] - len(dat))
    print("code=%d data(init)=%d data(total)=%d" % (len(code), sec[1]["unpackedLength"], len(dat)))

    import_addrs = [0x00FF0000 + i * 16 for i in range(ld.symCount)]
    hdr = ld.relocHeaders[0]
    ev = run_relocs(ld, hdr, True)
    dat = apply_relocs(dat, ev, code_base, data_base, sec_bases, import_addrs)

    toc = data_base + 0xB98
    print("r2 = 0x%08X" % toc)
    initOff = ld.initOffset
    e0 = u32(dat, initOff)
    t0 = u32(dat, initOff + 4)
    print("init tvec @data+0x%X = {entry=0x%08X, toc=0x%08X} (expect toc=0x%08X)" % (
        initOff, e0, t0, toc))
    print("init entry => code offset 0x%X" % (e0 - code_base))
    # stub -0xB7C target
    slot = 0xB98 - 0xB7C
    print("stub(-0xB7C) reads data+0x%X = 0x%08X (import base 0x%08X)" % (
        slot, u32(dat, slot), import_addrs[0]))
    # exported tvec for NQDDisposePixMap
    for name in ("NQDDisposePixMap", "CGSGetWindowShape", "PackBits"):
        if name in [k for k in ()]:
            pass
    # find one export
    area = ld.L + ld.exportHashOffset
    keyBase = area + (1 << ld.hashPower) * 4
    symBase = keyBase + ld.exportCount * 4
    for i in range(ld.exportCount):
        o = symBase + 10 * i
        cl = data[o]
        no = (data[o + 1] << 16) | (data[o + 2] << 8) | data[o + 3]
        val = u32(data, o + 4)
        s = struct.unpack_from(">h", data, o + 8)[0]
        if cl == 2 and s == 1 and i < 3:
            nm = data[ld.strings + no:data.find(b"\0", ld.strings + no)].decode("latin1")
            print("  export[%d] %-24s tvec@data+0x%X = {0x%08X, 0x%08X}" % (
                i, nm, val, u32(dat, val), u32(dat, val + 4)))


if __name__ == "__main__":
    main()
