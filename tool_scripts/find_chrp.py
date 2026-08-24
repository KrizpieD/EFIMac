data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\mac\Mac OS 9.2.2.iso", "rb").read()
needle = b"<CHRP-BOOT>"
pos = 0
hits = []
while True:
    pos = data.find(needle, pos)
    if pos < 0:
        break
    hits.append(pos)
    pos += 1
print("found", len(hits), "occurrences of <CHRP-BOOT>")
for h in hits:
    print("  offset 0x%X" % h)
print()
for h in hits:
    chunk = data[h:h+4096]
    print("=== 0x%X ===" % h)
    print(chunk[:200])
    print()
