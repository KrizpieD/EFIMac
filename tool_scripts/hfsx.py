import struct, sys

def be16(b, o): return struct.unpack_from(">H", b, o)[0]
def be32(b, o): return struct.unpack_from(">I", b, o)[0]
def be64(b, o): return struct.unpack_from(">Q", b, o)[0]

class ForkData:
    def __init__(self, b, o):
        self.logicalSize = be64(b, o)
        self.clumpSize = be32(b, o + 8)
        self.totalBlocks = be32(b, o + 12)
        self.extents = []
        for i in range(8):
            s = be32(b, o + 16 + i * 8)
            c = be32(b, o + 20 + i * 8)
            self.extents.append((s, c))

class HFSPlus:
    def __init__(self, data):
        self.data = data
        if len(data) < 2048:
            raise ValueError("too small")
        sig = be16(data, 1024)
        if sig not in (0x4244, 0x482B, 0x4858, 0x4858):
            raise ValueError("not HFS+ (sig %04x)" % sig)
        self.blockSize = be32(data, 1024 + 40)
        self.catalog = ForkData(data, 1024 + 272)
        self.blockShift = self.blockSize.bit_length() - 1

    def read_blocks(self, startBlock, count):
        base = startBlock * self.blockSize
        return self.data[base:base + count * self.blockSize]

    def read_fork(self, fork):
        out = bytearray()
        for (sb, bc) in fork.extents:
            out += self.read_blocks(sb, bc)
            if len(out) >= fork.logicalSize:
                break
        return bytes(out[:fork.logicalSize])

    def btree(self, fork):
        b = self.read_fork(fork)
        # header node is first node
        return BTree(b, self)

class BTree:
    def __init__(self, b, hfs):
        self.b = b
        self.hfs = hfs
        self.header = b[:512]
        # header node: nodeSize at offset 14 in the header record (offset 2 + 14? )
        # BTHeaderRec in header node: after 14 bytes of node descriptor
        self.nodeSize = be16(b, 2 + 14)
        self.maxKey = be16(b, 2 + 18)
        self.rootNode = be32(b, 2 + 24)

    def node(self, num):
        off = num * self.nodeSize
        return self.b[off:off + self.nodeSize]

    def walk(self, key, nodeNum=None):
        if nodeNum is None:
            nodeNum = self.rootNode
        node = self.node(nodeNum)
        kind = node[0]
        numRecs = be16(node, 4)
        if kind == 0x01:  # leaf
            return self.leaf_search(node, key)
        else:  # index node
            recs = []
            # record offsets start at nodeSize, descending: offset of record i at nodeSize - (i+1)*2
            for i in range(numRecs):
                off = self.nodeSize - (i + 1) * 2
                recOff = be16(node, off)
                klen = be16(node, recOff)
                kdata = node[recOff + 2:recOff + 2 + klen]
                child = be32(node, recOff + 2 + klen)
                recs.append((kdata, child))
            child = None
            for kdata, c in recs:
                # compare; catalog keys: compare raw bytes (they are big-endian fixed width)
                if kdata >= key:
                    child = c
                    break
            if child is None:
                child = recs[-1][1]
            return self.walk(key, child)

    def leaf_search(self, node, key):
        numRecs = be16(node, 4)
        for i in range(numRecs):
            off = self.nodeSize - (i + 1) * 2
            recOff = be16(node, off)
            klen = be16(node, recOff)
            kdata = node[recOff + 2:recOff + 2 + klen]
            rdata = node[recOff + 2 + klen:]
            yield kdata, rdata

def catalog_key(name, parentID, nodeName):
    # build comparable key: parentID (4 bytes BE) then name length+utf16be
    n = name
    nb = n.encode("utf-16-be")
    return struct.pack(">IH", parentID, len(nb)) + nb

def main(path, want):
    raw = open(path, "rb").read()
    h = HFSPlus(raw)

    def find(parentID, name):
        key = catalog_key(name, parentID, True)
        for kdata, rdata in h.btree(h.catalog).walk(key):
            # key: parentID(4) + nameLen(2) + name
            pid = be32(kdata, 0)
            if pid != parentID:
                continue
            nlen = be16(kdata, 4)
            nm = kdata[6:6 + nlen].decode("utf-16-be", "replace")
            if nm == name:
                return rdata
        return None

    # catalog record types: 1 folder, 2 file, 3 folder thread, 4 file thread
    folderID = 2  # root folder

    def rec_folder(fid, name):
        r = find(fid, name)
        if r is None:
            raise ValueError("folder not found: %s in %d" % (name, fid))
        ftype = be16(r, 0)
        if ftype == 3:  # folder thread -> returns id
            return be32(r, 4 + 4)  # record: type(2) reserved(2) thread(4)... skip
        # actual folder record: type 1
        # HFSPlusCatalogFolder: type(2) flags(2) valence(4) folderID(4) ...
        return be32(r, 8)

    # Walk: find "System Folder"
    systemID = rec_folder(folderID, "System Folder")

    # find "Mac OS ROM" file record
    r = find(systemID, want)
    if r is None:
        raise ValueError("file not found: %s" % want)
    ftype = be16(r, 0)
    if ftype != 2:
        raise ValueError("not a file record")
    # HFSPlusCatalogFile: type(2) flags(2) reserved(4) fileID(4) createDate(4) modDate(4)
    # fork start at offset 46? Layout:
    # 0 type, 2 flags, 4 reserved(4), 8 fileID, 12 create,16 modify,20 attrChange,24 accessDate,
    # 28 backupDate, 32 finderInfo(32), 64 textEncoding(4), 68 reserved(4), 72 dataFork(80), 152 rsrcFork(80)
    dataFork = ForkData(r, 72)
    out = h.read_fork(dataFork)
    print("logicalSize=%d blockSize=%d extents=%r" % (dataFork.logicalSize, h.blockSize, dataFork.extents))
    return out

if __name__ == "__main__":
    data = main(sys.argv[1], sys.argv[2])
    sys.stdout.buffer.write(data)
