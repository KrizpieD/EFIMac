#!/usr/bin/env python3
"""Map the PEF ('Joy!peffpwpc') containers in the System data fork.

Produces a per-fragment summary: file offsets/lengths, section headers, and the
loader-info header fields (mainSection, import/init/export counts, offsets),
used as groundwork for the paravirtual CFM-style OS loader.
"""
import sys


def rd32(b, off):
    return (b[off] << 24) | (b[off + 1] << 16) | (b[off + 2] << 8) | b[off + 3]


def rd16(b, off):
    return (b[off] << 8) | b[off + 1]


def rdstr(b, off, n=64):
    out = []
    for i in range(n):
        c = b[off + i]
        if c == 0:
            break
        out.append(chr(c) if 0x20 <= c < 0x7F else '.')
    return ''.join(out)


MAGIC = b'Joy!peffpwpc'


def find_containers(data):
    containers = []
    start = 0
    idx = data.find(MAGIC, 0)
    while idx != -1:
        containers.append(idx)
        idx = data.find(MAGIC, idx + 1)
    return containers


def parse(data, off, end):
    tag1 = data[off:off + 4]
    tag2 = data[off + 4:off + 8]
    arch = data[off + 8:off + 12]
    fmtver = rd32(data, off + 12)
    timestamp = rd32(data, off + 16)
    defver = rd32(data, off + 20)
    impver = rd32(data, off + 24)
    curver = rd32(data, off + 28)
    seccount = rd16(data, off + 32)
    instcount = rd16(data, off + 34)
    reserved = rd32(data, off + 36)
    if tag1 != b'Joy!' or tag2 != b'peff' or arch != b'pwpc':
        return None
    if seccount > 64:
        return None
    out = {
        'off': off,
        'end': end,
        'len': end - off,
        'fmtver': fmtver,
        'timestamp': timestamp,
        'curver': curver,
        'seccount': seccount,
        'instcount': instcount,
        'reserved': reserved,
        'sections': [],
        'loader': None,
    }
    for i in range(seccount):
        so = off + 40 + 28 * i
        out['sections'].append({
            'nameOff': rd32(data, so + 0),
            'defaultAddress': rd32(data, so + 4),
            'totalSize': rd32(data, so + 8),
            'unpackedSize': rd32(data, so + 12),
            'packedSize': rd32(data, so + 16),
            'containerOffset': rd32(data, so + 20),
            'hash': rd32(data, so + 24),
        })
    # Loader info header follows the last section data extent; we scan the
    # byte window after the section-header table for a self-consistent 78-byte
    # linker-info header and validate against the section data.
    scan_from = off + 40 + 28 * seccount
    if seccount == 0:
        return out
    out['loader'] = find_loader(data, scan_from, end)
    return out


def find_loader(data, start, end):
    """Brute-force a plausible 78-byte LoaderInfoHeader.
    Fields: mainSection(u16) libraryCount(u16) symbolDictOffset,length
    relocInstrOffset,count importLibOffset,count importSymbolOffset,count
    initRoutineOffset,count exportHashOffset,count exportHashTablePower(u16)
    exportDictOffset,count reserved(6).  Offsets are relative to this header.
    """
    for p in range(start, min(end, start + 0x4000) - 78, 1):
        mainsec = rd16(data, p)
        libcnt = rd16(data, p + 2)
        if libcnt > 256 or mainsec >= 8:
            continue
        sdO = rd32(data, p + 4)
        sdL = rd32(data, p + 8)
        relO = rd32(data, p + 12)
        relC = rd32(data, p + 16)
        ilO = rd32(data, p + 20)
        ilC = rd32(data, p + 24)
        isO = rd32(data, p + 28)
        isC = rd32(data, p + 32)
        inO = rd32(data, p + 36)
        inC = rd32(data, p + 40)
        ehO = rd32(data, p + 44)
        ehC = rd32(data, p + 48)
        power = rd16(data, p + 52)
        edO = rd32(data, p + 54)
        edC = rd32(data, p + 58)
        if power > 16 or inC > 256:
            continue
        # All non-zero offsets should land inside the immediate container area.
        ok = True
        for lbl, off, cnt in (('reloc', relO, relC), ('impt', ilO, ilC),
                              ('imps', isO, isC), ('init', inO, inC),
                              ('symd', sdO, sdL)):
            if off >= 0x10000:
                ok = False
                break
        if not ok:
            continue
        # Relative target must be within +/-32KB (real PEF uses this).
        def inside(o):
            return o != 0 and abs(o) < 0x20000
        if not (inside(relO) or inside(ilO) or inside(isO) or inside(edO)):
            continue
        return {
            'ofs': p,
            'mainSection': mainsec,
            'libraryCount': libcnt,
            'symbolDictOffset': sdO, 'symbolDictLength': sdL,
            'relocInstrOffset': relO, 'relocInstrCount': relC,
            'importLibOffset': ilO, 'importLibCount': ilC,
            'importSymbolOffset': isO, 'importSymbolCount': isC,
            'initRoutineOffset': inO, 'initRoutineCount': inC,
            'exportHashOffset': ehO, 'exportHashCount': ehC,
            'exportHashTablePower': power,
            'exportDictOffset': edO, 'exportDictCount': edC,
        }
    return None


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else r'%TEMP%'
    path = path.replace('%TEMP%', r'C:\Users\clayc\AppData\Local\Temp\opencode')
    data = open(path.replace('%TEMP%', r'C:\Users\clayc\AppData\Local\Temp\opencode'), 'rb').read() if path.startswith('%TEMP%') else open(path, 'rb').read()
    offs = find_containers(data)
    print(f'system_data_fork {len(data)} bytes; {len(offs)} PEF containers')
    frags = [parse(data, offs[i], offs[i + 1] if i + 1 < len(offs) else len(data))
             for i in range(len(offs))]
    row = 0
    for f in frags:
        if f is None:
            row += 1
            print(f'[{row:3}] at 0x{offs[row-1]:X}: BAD HEADER')
            continue
        row += 1
        tag = 'P1' if f['fmtver'] == 1 else str(f['fmtver'])
        ldr = f['loader']
        extra = ''
        if ldr:
            extra = (f'main={ldr["mainSection"]} lib={ldr["libraryCount"]} '
                     f'reloc={ldr["relocInstrCount"]} impS={ldr["importSymbolCount"]} '
                     f'init={ldr["initRoutineCount"]} expD={ldr["exportDictCount"]}')
        else:
            extra = 'loader=??'
        print(f'[{row:3}] @0x{f["off"]:06X} len=0x{f["len"]:07X} v{tag} '
              f'cur={f["curver"]} sec={f["seccount"]} inst={f["instcount"]} '
              f'{extra}')
        for si, s in enumerate(f['sections']):
            print(f'      sec[{si}] nameOff=0x{s["nameOff"]:08X} '
                  f'def@0x{s["defaultAddress"]:08X} tot=0x{s["totalSize"]:04X} '
                  f'unp=0x{s["unpackedSize"]:04X} pkd=0x{s["packedSize"]:04X} '
                  f'cont@0x{s["containerOffset"]:04X} hash=0x{s["hash"]:08X}')


if __name__ == '__main__':
    main()