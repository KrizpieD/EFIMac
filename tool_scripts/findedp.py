rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
import re
# find all ASCII-ish runs and report those ending near a 0xF00 page boundary
for m in re.finditer(rb"[\x20-\x7e]{6,}", rom):
    s, e = m.start(), m.end()
    # candidate: string sits at template+0xf00 => s % 0x1000 == 0xf00 (or close)
    if 0xEE0 <= (s & 0xFFF) <= 0xF10:
        txt = rom[s:s+min(40,e-s)]
        print(f"@{s:#x} (page {(s&~0xFFF):#x} +{(s&0xFFF):#x}): {txt!r}")
