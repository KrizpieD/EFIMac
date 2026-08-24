import subprocess, sys
base = [sys.executable, r'C:\Users\clayc\AppData\Local\Temp\opencode\ppcdis.py']
for lo, hi, name in [(0x40B12000, 0x40B12500, 'memdetect')]:
    p = subprocess.run(base + [hex(lo), hex(hi)], capture_output=True, text=True)
    out = []
    prev = None
    for l in p.stdout.splitlines():
        if l != prev:
            out.append(l)
            prev = l
    open(r'C:\Users\clayc\AppData\Local\Temp\opencode\dis_%s.txt' % name, 'w').write('\n'.join(out))
    print(name, 'lines', len(out))
