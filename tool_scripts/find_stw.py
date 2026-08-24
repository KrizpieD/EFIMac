import re
data = open(r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin','rb').read()
def find(bpat, label):
    hits = [m.start() for m in re.finditer(bpat, data, re.DOTALL)]
    print(label, ['0x%08x' % (h + 0x40800000) for h in hits][:25])
find(b'\x93[\x80-\xff]\x08\x0c', 'stw rD,0x80C(r31):')
find(b'\x93[\x80-\xff]\x07\x2c', 'stw rD,0x72C(r31):')
find(b'\x93[\x80-\xff]\x08\x18', 'stw rD,0x818(r31):')
find(b'\x83[\x80-\xff]\x08\x0c', 'lwz rD,0x80C(r31):')
