import re
log = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt", encoding="utf-8", errors="replace").read()
lines = log.splitlines()
# find first PRE / DBG / PROBE lines mentioning 0x40B10000-0x40B10200
seen = 0
for l in lines:
    if re.search(r'0x40B10[0-1][0-9A-F]{2}', l):
        print(l)
        seen += 1
        if seen > 40: break
