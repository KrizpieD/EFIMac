import struct
unpack_from = struct.unpack_from
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
n = len(data)

RANGES = [(0x00000000, 0x01000000),
          (0x20000000, 0x21100000),
          (0x21000000, 0x23000000),
          (0x40800000, 0x41000000),
          (0x68000000, 0x69000000)]

def ok_dest(a):
    return any(lo <= a < hi for lo, hi in RANGES)

def sim(start, cap=30000):
    mem = {}
    a3 = start
    maxw = start
    unpack = struct.unpack_from
    for _ in range(cap):
        a2 = a3
        while True:
            if a3 + 12 > n:
                return None
            d7 = unpack(">I", data, a3)[0]
            d7 = (d7 + unpack(">I", data, a3 + 4)[0]) & 0xFFFFFFFF
            v = unpack(">I", data, a3 + 8)[0]
            m1 = mem.get(a3); m2 = mem.get(a3 + 4); m3 = mem.get(a3 + 8)
            if m1 is not None: d7 = m1
            if m2 is not None: d7 = (d7 - d7) | 0  # rare; ignore correctness here
            if m2 is not None: d7 = m2 + unpack(">I", data, a3)[0]
            if m3 is not None: v = m3
            a3 += 12
            if d7 != v:
                break
        dv = mem.get(a2)
        if dv is None:
            dv = unpack(">I", data, a2)[0] if a2 + 4 <= n else 0
        d7 = (d7 - dv) & 0xFFFFFFFF
        mem[a2] = d7
        if a2 > maxw:
            maxw = a2
        a3 -= 4
        sv = mem.get(a3)
        if sv is None:
            sv = unpack(">I", data, a3)[0] if a3 + 4 <= n else 0
        if sv == 0xFFFFFFFF:
            return (a3 - start, maxw - start)
    return None

found = []
off = 0x200
while off < n - 16:
    b, s = unpack_from(">II", data, off)
    if 0x1000 <= s <= 0x200000 and ok_dest(b):
        end = b + ((s >> 2) * 3 if s < 0x200000 else 0x180000)
        if ok_dest(end - 1):
            st = sim(off)
            if st:
                found.append((off, b, s, st[0], st[1]))
    off += 4

real = [t for t in found if t[3] >= 0x400 and t[4] >= 0x200]
print(f"{len(found)} raw / {len(real)} substantial")
for off, b, s, term, mw in sorted(real, key=lambda t: t[3])[:60]:
    end = b + ((s >> 2) * 3 if s < 0x200000 else 0x180000)
    print(f"@{off:#08x} base={b:#010x} size={s:#08x} end={end:#010x} "
          f"term@+{term:#x} maxwr@+{mw:#x}")
