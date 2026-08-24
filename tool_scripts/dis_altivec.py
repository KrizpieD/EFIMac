import sys
sys.path.insert(0, r"C:\Users\clayc\Desktop\New folder (2)\EFIMac\tools")
import hfs_read
from capstone import *
from capstone.ppc import *

img = hfs_read.Image(r"C:\Users\clayc\AppData\Local\Temp\opencode\mac_disc\Mac_OS_9.2.2.iso")
vol, err = hfs_read.mount(img)
rec = None
def find(cid):
    global rec
    for kid in vol.children_of(cid):
        name = kid.get("name", "?")
        if "dnum" in kid:
            if find(kid["dnum"]): return True
        else:
            if name.lower() == "mac os rom":
                rec = kid; return True
    return False
find(2)
data = vol.read_file(rec)
open(r"C:\Users\clayc\AppData\Local\Temp\opencode\mac_os_rom.bin", "wb").write(data)
print("wrote", len(data))

BASE = 0x40800000
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
md.detail = True

for (lo, hi) in [(0x40B10600, 0x40B11600), (0x40B1F800, 0x40B1FC00)]:
    print(f"==== disasm {lo:#x}..{hi:#x} ====")
    off = lo - BASE
    for i in md.disasm(data[off:off + (hi - lo)], lo):
        print("0x%08x  %-10s %s" % (i.address, i.mnemonic, i.op_str))
