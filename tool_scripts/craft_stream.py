import struct

# Faithful transcription of ROM decompressor @0x40800624 (verified disasm):
#   64a: a3 = a6
#   64c: a2 = a3                    <- outer head
#   64e: d7 = M[a3]; a3 += 4
#   650: d7 += M[a3]; a3 += 4       <- inner head (beq target)
#   652: cmp M[a3], d7; a3 += 4
#   654: beq 650
#   656: d7 -= M[a2]; a2 += 4
#   658: M[a2] = d7; a2 += 4
#   65a: a3 -= 4
#   65c: if M[a3] != 0xFFFFFFFF: goto 64c
#   664: copy 16 bytes (a3+ -> a2+)
#   66c: M[a6-8] = d7 - a6 ; rts
#
# Key consequence: an isolated (non-run) group NETS BACKWARD one slot;
# forward progress requires cumulative-sum runs. Termination craft: build a
# full run chain whose final failed compare leaves a3 on the sentinel slot.

def sim(words, limit=100000):
    mem = {}
    for i, w in enumerate(words):
        mem[i * 4] = w
    def rd(off):
        return mem.get(off, 0)
    a3 = 0
    for _ in range(limit):
        a2 = a3
        d7 = rd(a3); a3 += 4
        while True:
            d7 = (d7 + rd(a3)) & 0xFFFFFFFF; a3 += 4
            v = rd(a3); a3 += 4
            if d7 != v:
                break
        d7 = (d7 - rd(a2)) & 0xFFFFFFFF
        a2 += 4
        mem[a2] = d7
        a2 += 4
        a3 -= 4
        if rd(a3) == 0xFFFFFFFF:
            return ("OK", a3, dict(mem))
    return ("SPIN", None, dict(mem))

def craft(base, size, n_run_pairs=4):
    ws = []
    ws.append(base)                 # idx0 header base
    ws.append(size)                 # idx1 header size
    acc = (base + size) & 0xFFFFFFFF
    for j in range(n_run_pairs):
        add_val = (0x11111111 * (j + 1)) & 0xFFFFFFFF
        ws.append(add_val)          # idx odd: added into acc
        acc = (acc + add_val) & 0xFFFFFFFF
        ws.append(acc)              # idx even: must equal acc to keep run
    ws.append(0xFFFFFFFF)           # sentinel: added (harmless), then cmp fails
    acc_sent = (acc + 0xFFFFFFFF) & 0xFFFFFFFF
    ws.append(acc_sent ^ 1)         # breaker != acc_sent guaranteed
    ws += [0xAA55AA55, 0x12345678, 0xFEEDFACE, 0x0BADF00D]  # 16-byte tail
    return ws, acc

def sp_target(base, size):
    return base + ((size >> 2) * 3 if size < 0x200000 else 0x180000)

if __name__ == "__main__":
    BASE = 0x0002C000   # dest base inside 256K low RAM
    SIZE = 0x00002000   # -> final SP = base + (size>>2)*3 = 0x2D800
    ws, acc = craft(BASE, SIZE)
    st, sent_off, mem = sim(ws)
    print(f"stream: {len(ws)} longs = {len(ws)*4} bytes")
    for i, w in enumerate(ws):
        print(f"  [{i*2:#05x}] {w:08x}")
    print(f"sim: {st} sentinel@byte {sent_off:#x}")
    print(f"final SP would be {sp_target(BASE, SIZE):#x}")
    if st == "OK":
        open(r"C:\Users\clayc\Desktop\New folder (2)\EFIMac\tool_scripts\crafted_stream.bin",
             "wb").write(b"".join(struct.pack(">I", w) for w in ws))
        print("written crafted_stream.bin")
