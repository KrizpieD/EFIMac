import struct
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()

def sim(start, limit=200000):
    # memory model: copy stream region into dict, writes mutate
    mem = {}
    def rd(a):
        return mem.get(a) or struct.unpack_from(">I", data, a)[0]
    def wr(a, v):
        mem[a] = v
    a3 = start
    iters = 0
    while iters < limit:
        iters += 1
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
        if rd(a3) == 0xFFFFFFFF:
            return ("OK", iters)
    return ("SPIN", iters)

for off in range(0xAB4E, 0xAB62, 2):
    r = sim(off)
    print(f"{off:#x}: {r[0]} iters={r[1]}")
