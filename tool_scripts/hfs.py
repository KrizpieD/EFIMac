import struct, sys

def be16(b, o): return struct.unpack_from(">H", b, o)[0]
def be32(b, o): return struct.unpack_from(">I", b, o)[0]

def read_hfs(path):
    raw = open(path, "rb").read()
    o = 1024
    if be16(raw, o) != 0x4244:
        raise ValueError("no HFS MDB")
    alblksiz = be32(raw, o + 20)
    alblst = be16(raw, o + 28)
    ctflsize = be32(raw, o + 146)
    ctexts = [(be16(raw, o + 150 + i * 4), be16(raw, o + 152 + i * 4)) for i in range(3)]

    def alloc_start(block):
        # allocation block N starts at 512-block drAlBlSt + N * (alblksiz/512)
        return alblst * 512 + block * alblksiz

    def read_alloc(block, count):
        return raw[alloc_start(block):alloc_start(block) + count * alblksiz]

    def read_extents(exts):
        out = b""
        for sb, bc in exts:
            out += read_alloc(sb, bc)
        return out

    catalog = read_extents(ctexts)[:ctflsize]
    return catalog, alblksiz, alblst, raw, alloc_start

def catalog_scan(catalog):
    nodesize = be16(catalog, 32)  # header node: BTHeaderRec.nodeSize at offset 14+18=32
    if nodesize <= 0:
        nodesize = 512
    total_nodes = len(catalog) // nodesize
    recs = []  # (parentID, name, recordData)

    def walk(node_num):
        node = catalog[node_num * nodesize:(node_num + 1) * nodesize]
        ftype = node[0]
        numrecs = be16(node, 4)
        out = []
        for i in range(numrecs):
            rec_off = nodesize - (i + 1) * 2
            off = be16(node, rec_off)
            klen = node[off]
            key = node[off + 1:off + 1 + klen]
            data = node[off + 1 + klen:]
            parent = be32(key, 0)
            name = key[4:].decode("latin1")
            out.append((parent, name, data))
        return ftype, out

    # iterative walk using stack of node numbers
    stack = [0]  # header node is node 0
    # Find all leaf records by scanning every node (simple, nodes are small)
    leaves = []
    for num in range(total_nodes):
        node = catalog[num * nodesize:(num + 1) * nodesize]
        if not node:
            break
        ftype = node[0]
        if ftype == 0x01:  # leaf
            numrecs = be16(node, 4)
            for i in range(numrecs):
                rec_off = nodesize - (i + 1) * 2
                off = be16(node, rec_off)
                klen = node[off]
                key = node[off + 1:off + 1 + klen]
                data = node[off + 1 + klen:]
                parent = be32(key, 0)
                name = key[4:].decode("latin1", "replace")
                leaves.append((parent, name, data))
    return leaves

def get_file_data(raw, alloc_start, alblksiz, rec):
    rtype = be16(rec, 0)
    if rtype != 2:
        return None
    stblk = be16(rec, 8)
    lglen = be32(rec, 10)
    exts = [(be16(rec, 76 + i * 4), be16(rec, 78 + i * 4)) for i in range(3)]
    exts = [(stblk, (lglen + alblksiz - 1) // alblksiz)] + exts
    out = b""
    for sb, bc in exts:
        if bc == 0:
            continue
        out += raw[alloc_start(sb):alloc_start(sb) + bc * alblksiz]
        if len(out) >= lglen:
            break
    return out[:lglen]

def main(path, want):
    catalog, alblksiz, alblst, raw, alloc_start = read_hfs(path)
    leaves = catalog_scan(catalog)
    # build name map
    by_parent = {}
    for parent, name, rec in leaves:
        by_parent.setdefault(parent, {})[name] = rec
    # find System Folder: its folder record's parent is the root (1)
    sys_rec = None
    sys_id = None
    for parent, name, rec in leaves:
        if name == "System Folder" and be16(rec, 0) == 1:
            sys_id = be32(rec, 6)
            sys_rec = rec
            break
    if sys_rec is None:
        raise ValueError("System Folder not found")
    want_rec = None
    for parent, name, rec in leaves:
        if parent == sys_id and name == want:
            want_rec = rec
            break
    if want_rec is None:
        raise ValueError("file %s not found" % want)
    data = get_file_data(raw, alloc_start, alblksiz, want_rec)
    print("size=%d" % len(data), file=sys.stderr)
    return data

if __name__ == "__main__":
    d = main(sys.argv[1], sys.argv[2])
    sys.stdout.buffer.write(d)
