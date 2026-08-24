import os, re
root = os.path.join(os.environ['TEMP'], 'opencode', 'powermac-rom')
pats = re.compile(r'fe0a|fe04|emulator op|EmulatorOp|DREmulator|DR emulator', re.I)
for dirpath, dirs, files in os.walk(root):
    for f in files:
        p = os.path.join(dirpath, f)
        try:
            data = open(p, 'r', errors='replace').read()
        except OSError:
            continue
        for i, line in enumerate(data.splitlines()):
            if pats.search(line):
                print('%s:%d: %s' % (f, i+1, line.strip()[:150]))
