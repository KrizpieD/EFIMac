import struct

data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\mac\Mac OS 9.2.2.iso", "rb").read()


def decode_chrp(base, label):
    # find parcels-offset and parcels-size constants in the CHRP text header
    buf = data[base:base + 0x40000]
    text = buf.replace(b"\x00", b" ")
    # search ASCII for 'parcels-offset' and 'parcels-size'
    po = pc = None
    for key in (b"parcels-offset", b"parcels-size"):
        idx = buf.find(key)
        if idx < 0:
            print(label, "MISSING", key)
            continue
        # value follows after '='
        eq = buf.find(b"=", idx)
        line_end = buf.find(b"\n", eq)
        val = buf[eq + 1:line_end].strip()
        val = val.replace(b"\x00", b"").strip()
        try:
            v = int(val, 16)
        except ValueError:
            v = None
        if key == b"parcels-offset":
            po = v
        else:
            pc = v
        print(label, key.decode(), "= 0x%X" % v if v else val)
    if po is None or pc is None:
        return None
    parcels = data[base + po: base + po + pc]
    if parcels[:4] != b"prcl":
        print(label, "parcels region does not start with 'prcl'")
        return None
    return parcels


def lzss_decode(lzss):
    dict_buf = bytearray(0x1000)
    runmask = 0
    remaining = len(lzss)
    si = 0
    dict_idx = 0xFEE
    out = bytearray()
    while remaining >= 0:
        if runmask < 0x100:
            remaining -= 1
            if remaining < 0:
                break
            runmask = lzss[si] | 0xFF00
            si += 1
        if runmask & 1:
            remaining -= 1
            if remaining < 0:
                break
            c = lzss[si]
            si += 1
            dict_buf[dict_idx & 0xFFF] = c
            out.append(c)
            dict_idx = (dict_idx + 1) & 0xFFF
        else:
            remaining -= 1
            if remaining < 0:
                break
            idx = lzss[si]
            si += 1
            remaining -= 1
            if remaining < 0:
                break
            cnt = lzss[si]
            si += 1
            start = idx | ((cnt << 4) & 0xF00)
            n = (cnt & 0x0F) + 3
            for _ in range(n):
                c = dict_buf[start & 0xFFF]
                dict_buf[dict_idx & 0xFFF] = c
                out.append(c)
                start = (start + 1) & 0xFFF
                dict_idx = (dict_idx + 1) & 0xFFF
        runmask >>= 1
    return out


def be32(b, o):
    return struct.unpack(">I", b[o:o + 4])[0]


def extract_rom_parcel(parcels):
    off = 0x14
    while off != 0:
        nxt = be32(parcels, off)
        typ = be32(parcels, off + 4)
        lz = be32(parcels, off + 8)
        if typ == 0x726F6D20:
            return lzss_decode(parcels[off + lz:nxt]), off, nxt
        off = nxt
    return None, None, None


for base, label in [(0x2424600, "IMG1@0x2424600"), (0x18CC52A8, "IMG2@0x18CC52A8")]:
    parcels = decode_chrp(base, label)
    if parcels is None:
        continue
    rom, off, nxt = extract_rom_parcel(parcels)
    print(label, "rom parcel chain off 0x%X size 0x%X, decompressed %d bytes" % (off, nxt - off, len(rom)))

    def word(a):
        return struct.unpack(">I", rom[a:a + 4])[0]

    print("  0x00000: 0x%08X" % word(0))
    print("  0x26440: 0x%08X (guest 0x40826440, emulator ran 0x48000705)" % word(0x26440))
    print("  0x28A74: 0x%08X (guest 0x40828A74, emulator ran 0x83C1FBFC)" % word(0x28A74))
    print("  0x31000: 0x%08X (guest 0x40B10000, emulator ran 0x4800000C)" % word(0x31000))
    print("  0x326440: 0x%08X" % word(0x326440))
    print("  0x328A74: 0x%08X" % word(0x328A74))
    print()
