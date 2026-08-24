import re
data = open(r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin','rb').read()
# stw rS,0x80C(rA): word & 0xFFFF == 0x080C, opcode 0x90...
def find_offs(off, label, limit=30):
    hits = []
    for i in range(0, len(data)-4, 2):
        if data[i] == 0x90 and data[i+3] == (off & 0xFF) and data[i+2] == ((off >> 8) & 0xFF):
            hits.append(i)
    print(label, ['0x%08x' % (h + 0x40800000) for h in hits[:limit]])
find_offs(0x080C, 'stw rS,0x80C(rA):')
find_offs(0x072C, 'stw rS,0x72C(rA):')
find_offs(0x0818, 'stw rS,0x818(rA):')
find_offs(0x0814, 'stw rS,0x814(rA):')
find_offs(0x8074, 'stw rS,0x8074(rA) (ed+0x74):')
