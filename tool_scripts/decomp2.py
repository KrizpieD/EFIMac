import struct

data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\mac\Mac OS 9.2.2.iso", "rb").read()


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


base = 0x18CC52A8
# assume the whole remaining file is LZSS, decompress to find CHRP structure
start = data.find(b"<CHRP-BOOT>", 0x18CC52A8 - 0x300000)
print("prior plain <CHRP-BOOT> at", hex(start) if start >= 0 else None)
raw = data[0x18CC52A8:]
dec = lzss_decode(raw)
print("decompressed %d bytes (first copy of header): %s" % (len(dec), dec[:80]))
