import os, re, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
root = os.path.join(os.environ['TEMP'], 'opencode', 'powermac-rom')
pats = re.compile(r'-0x20\(r1\)|-32\(r1\)|CurAS|CurrentAS|AddressSpace.*equ|equ.*-32', re.I)
for dirpath, dirs, files in os.walk(root):
    for f in files:
        p = os.path.join(dirpath, f)
        try:
            data = open(p, 'r', errors='replace').read()
        except OSError:
            continue
        lines = data.splitlines()
        for i, line in enumerate(lines):
            if pats.search(line):
                print('%s:%d: %s' % (os.path.basename(f), i+1, line.strip()[:160]))
