import re
lines = open(r'C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt', encoding='utf-8', errors='replace').read().splitlines()
for ln in lines:
    m = re.search(r'PROGRESS\[(\d+)\] PC=0x40B12([0-9A-F]{3}) LR=0x[0-9A-F]+ r1=0x[0-9A-F]+ r8=0x[0-9A-F]+ r28=0x[0-9A-F]+ SPRG4=0x[0-9A-F]+', ln)
    if m:
        print(ln)
