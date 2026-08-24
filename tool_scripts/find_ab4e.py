import struct
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
n = len(data)

# 1) absolute immediate forms: move.l #0x4080AB4E,-(sp) => 2F 7C 40 80 AB 4E
#    also movea.l forms 20 7C / 24 7C / 26 7C / 28 7C / 2A 7C / 2C 7C / 2E 7C,
#    and lea 41 FC.. etc.
pat = bytes([0x40, 0x80, 0xAB, 0x4E])
print("absolute const occurrences:")
o = data.find(pat)
while o != -1:
    ctx = data[o-2:o+6]
    print(f"  @{o:#07x} prev2={ctx[0]:02x}{ctx[1]:02x} "
          f"full={data[max(0,o-4):o+8].hex()}")
    o = data.find(pat, o + 1)

# 2) pc-relative: opcode word at p with disp at p+2 resolves to AB4E
ops = {0x41FA:"lea(pc),a0", 0x43FA:"lea(pc),a1", 0x45FA:"lea(pc),a2",
       0x47FA:"lea(pc),a3", 0x207A:"movea.l(pc),a0", 0x247A:"a1",
       0x267A:"a2", 0x287A:"a3", 0x2A7A:"a4", 0x2C7A:"a5",
       0x487A:"pea(pc)", 0x2F3B:"move.l dN,(sp)"}
TARGET = 0xAB4E
print("pc-rel refs:")
hits = 0
for p in range(0, n - 4, 2):
    w = struct.unpack_from(">H", data, p)[0]
    if w in ops:
        d = struct.unpack_from(">h", data, p + 2)[0]
        if p + 2 + 2 + d == TARGET or p + 2 + d == TARGET:
            print(f"  @{p:#07x} {ops[w]} disp={d:+#06x} "
                  f"ctx={data[p:p+8].hex()}")
            hits += 1
print(f"  total {hits}")

# 3) who else references AB4E region indirectly: scan u32 == 0x4080AB4E
q = b"\x40\x80\xab\x4e"
o = data.find(q)
print("raw u32 hits:", end=" ")
while o != -1:
    print(f"@{o:#07x}", end=" ")
    o = data.find(q, o + 1)
print()
