#!/usr/bin/env python3
"""Prototype multi-fragment PEF/CFM loader.

Enumerates every PEF container in a data fork, parses sections + loader
tables, expands pattern-initialized data, builds a global export map, and
resolves each fragment's imports by library + symbol name.  Used to validate
the loading model before porting to C.
"""

import struct
import sys
import collections

sys.path.insert(0, r"C:\Users\clayc\Desktop\EFIMac\tool_scripts")
from pef_pattern import unpack_pattern
from pef_reloc import Loader


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def u16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def find_fragments(data):
    bases = []
    base = 0x2A0
    while base + 40 <= len(data) and data[base:base + 12] == b"Joy!peffpwpc":
        bases.append(base)
        n = u16(data, base + 32)
        end = base
        for i in range(n):
            o = base + 40 + 28 * i
            co = u32(data, o + 20)
            cl = u32(data, o + 16)
            end = max(end, base + co + cl)
        base = (end + 15) & ~15
    return bases


def cstr(data, off, limit=None):
    e = data.find(b"\0", off)
    if e < 0:
        e = off
    return data[off:e].decode("latin1")


class Fragment:
    def __init__(self, data, base):
        self.b = data
        self.base = base
        self.ld = Loader(data, base)
        self.ld.parse()
        self.sec = self.ld.sections
        self.name = self._first_name()
        self.imports = []
        self.exports = {}
        self._parse_exports()

    def _first_name(self):
        for s in self.sec:
            if s["nameOff"] >= 0 and s["kind"] != 4:
                nameTab = self.base + 40 + 28 * len(self.sec)
                return cstr(self.b, nameTab + s["nameOff"])
        return None

    def expand(self):
        out = []
        for s in self.sec:
            if s["kind"] == 2:
                out.append(unpack_pattern(self.b, self.base + s["containerOffset"],
                                          s["containerLength"], s["unpackedLength"]))
            else:
                out.append(None)
        return out

    def _parse_exports(self):
        ld = self.ld
        b, L = self.b, ld.L
        area = L + ld.exportHashOffset
        keyBase = area + (1 << ld.hashPower) * 4
        symBase = keyBase + ld.exportCount * 4
        for i in range(ld.exportCount):
            o = symBase + 10 * i
            cl = b[o]
            no = (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]
            val = u32(b, o + 4)
            sec = struct.unpack_from(">h", b, o + 8)[0]
            name = cstr(b, ld.strings + no)
            self.exports[name] = (cl, sec, val)

    def parse_imports(self):
        ld = self.ld
        self.imports = []
        for k, sym in enumerate(ld.symbols):
            lib = None
            for lb in ld.libs:
                if lb["firstSymbol"] <= k < lb["firstSymbol"] + lb["symbolCount"]:
                    lib = lb
                    break
            self.imports.append(dict(
                index=k, cls=sym["cls"],
                name=ld.name(sym["nameOffset"]),
                lib=ld.name(lib["nameOffset"]) if lib else None,
                libOptions=lib["options"] if lib else 0,
            ))


def main():
    try:
        sys.stdout.reconfigure(errors="replace")
    except Exception:
        pass
    path = sys.argv[1] if len(sys.argv) > 1 else \
        r"C:\Users\clayc\AppData\Local\Temp\opencode\system_data_fork.bin"
    data = open(path, "rb").read()
    bases = find_fragments(data)
    print("fragments found: %d" % len(bases))
    frags = []
    for base in bases:
        try:
            f = Fragment(data, base)
            f.parse_imports()
            frags.append(f)
        except Exception as e:
            print("  base 0x%X parse error: %s" % (base, e))
    print("parsed: %d" % len(frags))

    exportMap = collections.defaultdict(list)
    for fi, f in enumerate(frags):
        for name, (cl, sec, val) in f.exports.items():
            exportMap[name].append((fi, cl, sec, val))

    unnamed = sum(1 for f in frags if not f.name)
    print("named fragments: %d / %d" % (len(frags) - unnamed, len(frags)))
    print("\ntop exporters:")
    for fi, f in enumerate(frags[:20]):
        print("  frag[%2d] @0x%06X name=%-24s exports=%4d imports=%3d libs=%d" % (
            fi, f.base, f.name or "?", len(f.exports), len(f.imports), f.ld.libCount))

    # resolve imports across all fragments
    unresolved = collections.Counter()
    resolved = 0
    for f in frags:
        for imp in f.imports:
            cands = exportMap.get(imp["name"])
            if cands:
                resolved += 1
            else:
                key = (imp["lib"], imp["cls"])
                unresolved[key] += 1
    print("\nresolved imports: %d ; unresolved buckets:" % resolved)
    for (lib, cls), n in unresolved.most_common(25):
        print("   lib=%-24s cls=%d count=%d" % (lib, cls, n))


if __name__ == "__main__":
    main()
