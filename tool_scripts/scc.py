import re
f = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt", encoding="utf-8", errors="replace")
chars = []
for line in f:
    m = re.search(r"\[SCC\] putchar 0x([0-9a-fA-F]+)", line)
    if m:
        chars.append(int(m.group(1), 16))
text = "".join(chr(c) if 32 <= c < 127 else "\n" if c == 13 else "" for c in chars)
print(text[-3000:])
