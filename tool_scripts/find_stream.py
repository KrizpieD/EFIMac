import struct
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()

def sim(start, limit=300000):
    mem = {}
    def rd(a):
        return mem.get(a) or struct.unpack_from(">I", data, a)[0]
    def wr(a, v):
        mem[a] = v
    a3 = start
    writes = []
    for _ in range(limit):
        a2 = a3
        while True:
            d7 = rd(a3); a3 += 4
            d7 = (d7 + rd(a3)) & 0xFFFFFFFF; a3 += 4
            v = rd(a3); a3 += 4
            if d7 != v:
                break
        d7 = (d7 - rd(a2)) & 0xFFFFFFFF
        wr(a2, d7); a2 += 4
        a3 -= 4
        writes.append(a2)
        if rd(a3) == 0xFFFFFFFF:
            return ("OK", a3 - start, max(writes))
    return ("SPIN", limit, 0)

base_addr, size = struct.unpack_from(">II", data, 0xAB4E)
print(f"AB4E hdr: base={base_addr:#x} size={size:#x}")
for off in range(0xAB60, 0xC000, 4):
    b, s = struct.unpack_from(">II", data, off)
    if b == 0 or s == 0 or s > 0x200000 or b > 0x01000000 and not (0x68000000 <= b < 0x69000000):
        continue
    st = sim(off)
    if st[0] == "OK" and st[1] < 0x8000 and st[2] < off + st[1]:
        print(f"CANDIDATE {off:#x}: base={b:#x} size={s:#x} "
              f"term@+{st[1]:#x} maxwrite@+{st[2]-off:#x}")
