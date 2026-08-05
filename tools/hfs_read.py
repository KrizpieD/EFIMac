#!/usr/bin/env python3
"""Read classic HFS and HFS+ volumes (raw images, or Apple Partition Map disks)
and list / extract files.

On-disk layouts follow the Linux kernel's include/linux/hfs_common.h and the
old Inside Macintosh: Files B-tree format (14-byte bnode descriptor with 32-bit
links; records located via a u16 offset table at the bottom of each node).
Catalog record types are 1=directory, 2=file, 3=dir-thread, 4=file-thread.

The volume is auto-detected: a raw volume at offset 0 wins, otherwise an
Apple_HFS partition is used, otherwise the largest plausible MDB found on a
2048-byte boundary is chosen (this finds the "Mac OS 8.1HD" volume on the
retail 8.1 CD, which sits outside the declared APM partitions).

Usage:
  hfs_read.py <image> list
  hfs_read.py <image> extract <mac-path> [-o outfile] [--hex]
"""
import argparse, os, struct, sys

def u16(b, o): return struct.unpack_from(">H", b, o)[0]
def u32(b, o): return struct.unpack_from(">I", b, o)[0]

HFS_SIG = b"BD"
HFS_PLUS_SIG = b"H+"
HFS_PLUS_SIGX = b"HX"

# Catalog record types (hfs_common.h):
HFS_CDR_DIR = 0x01   # folder (directory)
HFS_CDR_FIL = 0x02   # file
HFS_CDR_THD = 0x03   # folder (directory) thread
HFS_CDR_FTH = 0x04   # file thread

# ---------------------------------------------------------------- block I/O
class Image:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.size = os.path.getsize(path)

    def read(self, off, n):
        if off < 0 or off + n > self.size:
            return b""
        self.f.seek(off)
        return self.f.read(n)


class APM:
    """Apple Partition Map block 0 = DDMap ('ER', block size @2), entries
    from block 1 ('PM'; pmPyPartStart@8, pmPartBlkCnt@12, name@16, type@48)."""
    def __init__(self, img):
        bs = u16(img.read(2, 2), 0)
        self.blocksize = bs if 512 <= bs <= 8192 else 2048
        self.parts = []
        entry = 1
        while True:
            eo = entry * self.blocksize
            if img.read(eo, 2) != b"PM":
                break
            name = img.read(eo + 16, 32).decode("latin1").split("\0")[0]
            ptype = img.read(eo + 48, 32).decode("latin1").split("\0")[0]
            pstart = u32(img.read(eo + 8, 4), 0)
            pblocks = u32(img.read(eo + 12, 4), 0)
            self.parts.append((name, ptype, pstart * self.blocksize,
                               pblocks * self.blocksize))
            entry += 1
            if entry > 128:
                break


# ================================================================== classic HFS
class HFSVolume:
    def __init__(self, img, base):
        self.img, self.base = img, base
        mdb = base + 1024
        self.alblk = u32(img.read(mdb + 20, 4), 0)         # drAlBlkSiz
        self.alblk_off = u16(img.read(mdb + 28, 2), 0) * 512  # drAlBlSt
        vn = img.read(mdb + 36, 28)
        n = vn[0]
        self.name = vn[1:1 + n].decode("latin1", "replace")
        self.cat_extents = self._extents(mdb + 150)      # drCTExtRec
        self.cat_size = u32(img.read(mdb + 146, 4), 0)   # drCTFlSize
        self.xof_extents = self._extents(mdb + 134)      # drXTExtRec
        self.xof_size = u32(img.read(mdb + 130, 4), 0)   # drXTFlSize
        self.cat = BTreeHFS(self, self._file_bytes(self.cat_extents, self.cat_size))
        self.xof = BTreeHFS(self, self._file_bytes(self.xof_extents, self.xof_size))
        self.files, self.dirs, self.threads = {}, {}, {}
        self._build_catalog()
        self._xof_index = self._build_xof_index()

    def _extents(self, off):
        out = []
        for i in range(3):
            block = u16(self.img.read(off + i * 4, 2), 0)
            count = u16(self.img.read(off + i * 4 + 2, 2), 0)
            if count:
                out.append((block, count))
        return out

    def _file_bytes(self, extents, size):
        data = bytearray()
        for block, count in extents:
            off = self.base + self.alblk_off + block * self.alblk
            data += self.img.read(off, count * self.alblk)
        return bytes(data[:size])

    # ---- catalog parsing (hfs_common.h structs) ----
    def _build_catalog(self):
        for key, rec in self.cat.leaves():
            if len(key) < 7:
                continue
            parent = u32(key, 1)          # ParID
            namelen = key[5]
            name = key[6:6 + namelen].decode("latin1", "replace")
            rtype = rec[0] if rec else 0
            if rtype == HFS_CDR_FIL:      # file (type 2)
                rec = rec[:102]
                flnum = u32(rec, 20)      # FlNum
                size = u32(rec, 26)       # LgLen
                exts = [(u16(rec, 74 + i * 4), u16(rec, 76 + i * 4)) for i in range(3)]
                exts = [(b, c) for b, c in exts if c]
                self.files[flnum] = dict(parent=parent, name=name, flnum=flnum,
                                         size=size, extents=exts)
            elif rtype == HFS_CDR_DIR:    # directory (type 1)
                rec = rec[:70]
                dnum = u32(rec, 6)        # DirID
                self.dirs[dnum] = dict(parent=parent, name=name, dnum=dnum)

    def _build_xof_index(self):
        """Map (FlNum, FkType) -> sorted list of (FABN, extents)."""
        idx = {}
        for key, rec in self.xof.leaves():
            if len(key) < 7:
                continue
            fktype = key[0]
            flnum = u32(key, 1)
            fabn = u16(key, 5)
            rec = rec[:12]
            exts = [(u16(rec, i * 4), u16(rec, i * 4 + 2)) for i in range(3)]
            exts = [(b, c) for b, c in exts if c]
            idx.setdefault((flnum, fktype), []).append((fabn, exts))
        for v in idx.values():
            v.sort()
        return idx

    def read_file(self, frec):
        out = bytearray()
        have = 0
        logical = 0
        exts = list(frec["extents"])
        xlst = sorted(self._xof_index.get((frec["flnum"], 0), []))
        while have < frec["size"]:
            while exts and have < frec["size"]:
                block, count = exts.pop(0)
                n = min(count * self.alblk, frec["size"] - have)
                out += self.img.read(self.base + self.alblk_off + block * self.alblk, n)
                have += n
                logical += count
            if have >= frec["size"]:
                break
            if not xlst:
                break
            fabn, exts = xlst.pop(0)
            if fabn > logical:
                break
        return bytes(out[:frec["size"]])

    def children_of(self, cid):
        kids = []
        for d in self.dirs.values():
            if d["parent"] == cid:
                kids.append(d)
        for f in self.files.values():
            if f["parent"] == cid:
                kids.append(f)
        return kids


# ---------------------------------------------------------------- HFS B-tree
class BTreeHFS:
    """Classic HFS B-tree held in memory. 14-byte node descriptor, 32-bit
    links, records found via a u16 offset table at the bottom of each node."""
    def __init__(self, vol, data):
        self.vol = vol
        self.data = data
        if len(data) < 56:
            self.node_size = 512
            self.root = 0
            self.max_key_len = 37
            return
        self.node_size = u16(data, 32)    # bthNodeSize
        self.root = u32(data, 16)         # bthRoot
        self.max_key_len = u16(data, 34)  # bthKeyLen
        self.attributes = u32(data, 52)
        self.nodes = len(data) // self.node_size

    def node(self, n):
        if n < 0 or n * self.node_size + self.node_size > len(self.data):
            return None
        off = n * self.node_size
        return self.data[off:off + self.node_size]

    def _rec_offsets(self, d):
        num_recs = u16(d, 10)
        return [u16(d, self.node_size - 2 - 2 * i) for i in range(num_recs)]

    def leaves(self):
        """Yield (key, full record bytes) for every record in every leaf node.

        Classic HFS stores the key length byte (content length = 6+namelen)
        and pads the total key (length byte + content) to an even size; the
        kernel reads that padded size as (key_len_byte | 1) + 1."""
        for n in range(self.nodes):
            d = self.node(n)
            if d is None or d[8] != 0xFF:
                continue
            for roff in self._rec_offsets(d):
                if roff + 1 >= self.node_size:
                    continue
                klen_byte = d[roff]
                if klen_byte == 0:
                    continue
                key_size = (klen_byte | 1) + 1
                if roff + key_size > self.node_size:
                    continue
                key = d[roff + 1:roff + 1 + klen_byte]
                yield key, d[roff + key_size:]


# ================================================================== HFS+
class HFSPlusVolume:
    def __init__(self, img, base):
        self.img, self.base = img, base
        vh = base + 1024
        self.block_size = u32(img.read(vh + 42, 4))
        self.total_blocks = u32(img.read(vh + 46, 4))
        catlog_size, cat_exts = self._fork(vh + 274)
        self.cat = BTreeHFSPlus(self, cat_exts, catlog_size)
        self.files, self.dirs = {}, {}
        self._build_catalog()

    def _fork(self, off):
        logical = struct.unpack_from(">Q", self.img.read(off, 8), 0)[0]
        exts = []
        for i in range(8):
            sb = u32(self.img.read(off + 16 + i * 8, 4), 0)
            cb = u32(self.img.read(off + 20 + i * 8, 4), 0)
            if cb:
                exts.append((sb, cb))
        return logical, exts

    def _read_extents(self, exts, size):
        data = bytearray()
        for sb, cb in exts:
            data += self.img.read(self.base + sb * self.block_size, cb * self.block_size)
        return bytes(data[:size])

    def _build_catalog(self):
        for key, rec in self.cat.leaves():
            if len(key) < 4:
                continue
            parent = u32(key, 0)
            nlen = (len(key) - 4) // 2
            name = key[4:4 + nlen * 2].decode("utf-16-be", "replace")
            rtype = u16(rec, 0)
            if rtype == 1:                      # HFSPLUS_FOLDER
                dnum = u32(rec, 8)
                self.dirs[dnum] = dict(parent=parent, name=name, dnum=dnum)
            elif rtype == 2:                    # HFSPLUS_FILE
                flnum = u32(rec, 8)
                size = struct.unpack_from(">Q", rec, 88)[0]
                exts = [(u32(rec, 104 + i * 8), u32(rec, 108 + i * 8)) for i in range(8)]
                exts = [(b, c) for b, c in exts if c]
                self.files[flnum] = dict(parent=parent, name=name, flnum=flnum,
                                         size=size, extents=exts)

    def read_file(self, frec):
        out = bytearray()
        have = 0
        for sb, cb in frec["extents"]:
            if have >= frec["size"]:
                break
            n = min(cb * self.block_size, frec["size"] - have)
            out += self.img.read(self.base + sb * self.block_size, n)
            have += cb * self.block_size
        return bytes(out[:frec["size"]])

    def children_of(self, cid):
        kids = []
        for d in self.dirs.values():
            if d["parent"] == cid:
                kids.append(d)
        for f in self.files.values():
            if f["parent"] == cid:
                kids.append(f)
        return kids


class BTreeHFSPlus:
    """HFS+ B-tree: 14-byte node descriptor (same as HFS), keys are u16-length."""
    def __init__(self, vol, exts, logical_size):
        self.vol = vol
        self.data = vol._read_extents(exts, logical_size)
        self.node_size = 512
        self.nodes = len(self.data) // self.node_size
        if len(self.data) >= 82:
            self.node_size = u16(self.data, 14 + 18)
            self.root = u32(self.data, 14 + 2)
            self.nodes = len(self.data) // self.node_size

    def node(self, n):
        if n < 0 or n * self.node_size + self.node_size > len(self.data):
            return None
        off = n * self.node_size
        return self.data[off:off + self.node_size]

    def leaves(self):
        for n in range(self.nodes):
            d = self.node(n)
            if d is None or d[8] != 0xFF:
                continue
            num_recs = u16(d, 10)
            for i in range(num_recs):
                roff = u16(d, self.node_size - 2 - 2 * i)
                if roff + 2 >= self.node_size:
                    continue
                klen = u16(d, roff)
                if klen == 0 or roff + 2 + klen >= self.node_size:
                    continue
                key = d[roff + 2:roff + 2 + klen]
                rec = d[roff + 2 + klen:]
                rtype = u16(rec, 0) if len(rec) >= 2 else 0
                rlen = 328 if rtype == 2 else 88 if rtype == 1 else 0
                yield key, rec[:rlen]


# ---------------------------------------------------------------- top level
def fmt_size(n):
    for u in ("B", "KB", "MB"):
        if n < 1024 or u == "MB":
            return f"{n:.1f} {u}"
        n /= 1024
    return f"{n} B"


def _mdb_fields(img, base):
    """Return (sig, size, name) if a plausible HFS MDB sits at base+1024."""
    mdb = base + 1024
    if mdb + 64 > img.size:
        return None
    hdr = img.read(mdb, 64)
    if hdr[0:2] not in (HFS_SIG, HFS_PLUS_SIG, HFS_PLUS_SIGX):
        return None
    alblk = u32(hdr, 20)              # drAlBlkSiz
    nblks = u16(hdr, 18)              # drNmAlBlks
    vlen = hdr[36]
    if alblk % 512 or not (512 <= alblk <= 1 << 20):
        return None
    if not (1 <= vlen <= 31):
        return None
    if nblks <= 0 or nblks * alblk > img.size:
        return None
    name = hdr[37:37 + vlen].decode("latin1", "replace")
    return hdr[0:2], nblks * alblk, name


def detect(img):
    m = _mdb_fields(img, 0)
    if m:
        return m[0], 0, None
    candidates = []
    if img.read(0, 2) == b"ER" or img.read(1024, 2) == b"PM":
        try:
            apm = APM(img)
        except Exception:
            apm = None
        if apm:
            for name, ptype, pstart, psize in apm.parts:
                if ptype.startswith("Apple_HFS"):
                    m = _mdb_fields(img, pstart)
                    if m:
                        candidates.append((m[1], m[0], pstart, name))
    if not candidates:
        step = 2048
        for off in range(0, img.size - 2048, step):
            m = _mdb_fields(img, off)
            if m:
                candidates.append((m[1], m[0], off, None))
    if candidates:
        candidates.sort(key=lambda c: c[0], reverse=True)
        _, sig, base, name = candidates[0]
        return sig, base, name
    return None, 0, None


def mount(img):
    sig, base, pname = detect(img)
    if sig is None:
        return None, "Unrecognized image (no APM/HFS/HFS+ signature)"
    if sig == HFS_SIG:
        return HFSVolume(img, base), None
    if sig == HFS_PLUS_SIGX:
        return None, "HFSX (case-sensitive) not supported"
    return HFSPlusVolume(img, base), None


def main(argv):
    ap = argparse.ArgumentParser(prog="hfs_read.py")
    ap.add_argument("image")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    ex = sub.add_parser("extract")
    ex.add_argument("path")
    ex.add_argument("-o", dest="out")
    ex.add_argument("--hex", action="store_true")
    args = ap.parse_args(argv)

    img = Image(args.image)
    vol, err = mount(img)
    if vol is None:
        print(err)
        return 1
    sig, base, pname = detect(img)
    print(f"Volume: {vol.name}  format={'HFS' if sig == HFS_SIG else 'HFS+'}  "
          f"blocksize={vol.alblk if sig == HFS_SIG else vol.block_size}  "
          f"partition={pname}")

    if args.cmd == "list":
        def walk(cid, prefix):
            for kid in vol.children_of(cid):
                if "dnum" in kid:
                    print(f"{prefix}{kid['name']}/")
                    walk(kid["dnum"], prefix + kid["name"] + "/")
                else:
                    print(f"{prefix}{kid['name']}  ({fmt_size(kid['size'])})")
        walk(2, "")
        return 0

    segs = [s for s in args.path.replace("/", "\\").split("\\") if s]
    node = None
    for i, seg in enumerate(segs):
        if i == 0:
            for kid in vol.children_of(2):
                if kid["name"].lower() == seg.lower():
                    node = kid
                    break
        else:
            if node is None or "dnum" not in node:
                node = None
                break
            nxt = None
            for kid in vol.children_of(node["dnum"]):
                if kid["name"].lower() == seg.lower():
                    nxt = kid
                    break
            node = nxt
    if node is None:
        print(f"Path not found: {args.path}")
        return 1
    if "dnum" in node:
        print("Target is a directory; directory extraction not supported")
        return 1
    data = vol.read_file(node)
    if args.hex:
        for i in range(0, min(len(data), 256), 16):
            chunk = data[i:i + 16]
            print(f"{i:08x}  " + " ".join(f"{b:02x}" for b in chunk))
        return 0
    out = args.out or node["name"]
    with open(out, "wb") as f:
        f.write(data)
    print(f"Extracted {node['name']}: {len(data)} bytes -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
