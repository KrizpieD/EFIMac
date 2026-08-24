import os, re
root = os.path.join(os.environ['TEMP'], 'opencode', 'powermac-rom')
pats = re.compile(r'CurAS|PerProcessor|per-processor|-0x20|\bFFFC\b|kdp-4|kdp-8|kdp-10|kdp-14|kdp-18|kdp-1c|kdp-20', re.I)
for dirpath, dirs, files in os.walk(root):
    for f in files:
        p = os.path.join(dirpath, f)
        try:
            data = open(p, 'r', errors='replace').read()
        except OSError:
            continue
        for i, line in enumerate(data.splitlines()):
            if pats.search(line):
                print('%s:%d: %s' % (os.path.basename(f), i+1, line.strip()[:150]))
