import os
from capstone import *
p = os.path.join(os.environ['TEMP'], 'opencode', 'macosrom_flat.bin')
data = open(p, 'rb').read()
target = 0x408047AE

# scan whole image for word-relative branches landing on target
import struct
hits = []
for off in range(0, len(data) - 4, 2):
    w = struct.unpack('>H', data[off:off + 2])[0]
    base = 0x40800000 + off
    if (w & 0xFF00) == 0x6000:  # BRA/Bcc.S
        disp = w & 0xFF
        if disp >= 0x80:
            disp -= 256
        if base + 2 + disp == target and disp != -1 and disp != 1:
            hits.append((hex(base), f'BccS+{disp}'))
    if (w & 0xF0FF) == 0x6000 and False:
        pass
# BRA.W 6000xxxx / BSR 6100 / Bcc.W 6xxx with 16-bit disp
for off in range(0, len(data) - 4, 2):
    w = struct.unpack('>H', data[off:off + 2])[0]
    base = 0x40800000 + off
    if (w & 0xFF00) == 0x6000 and (w & 0xFF) == 0x00 and (w & 0x0F00) != 0x0100:
        disp = struct.unpack('>h', data[off + 2:off + 4])[0]
        if base + 2 + disp == target:
            hits.append((hex(base), 'BccW'))
print('branch sources:', hits[:20])
print('total:', len(hits))
