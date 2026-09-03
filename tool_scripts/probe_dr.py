import struct
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
# find usable PPC disassembler in tool_scripts
import importlib.util, sys
# Try ppc_dis approach - just implement a minimal decoder for the DR dispatch
print("=== bytes at 0x40B67B60 (DR dispatch) ===")
off = 0x40B67B60 - 0x40800000
print(data[off:off+0x120].hex(" "))
