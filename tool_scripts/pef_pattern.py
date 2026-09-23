#!/usr/bin/env python3
"""PEF pattern-initialized data (pidata) interpreter.

Implements the opcode set from "Mac OS Runtime Architectures", Chapter 8,
"Pattern-Initialized Data" (RTArch-94).  Instruction byte = (opcode<<5)|count;
count==0 means the count is a following varint argument.  Arguments are 7-bit
big-endian varints terminated by a byte whose high bit is clear.

Usage: pef_pattern.py <file> <containerOffset> <patternLen> <unpackedSize>
"""

import struct
import sys


def rd32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def read_varint(b, p):
    val = 0
    while True:
        c = b[p]
        p += 1
        val = (val << 7) | (c & 0x7F)
        if not (c & 0x80):
            break
    return val, p


def read_arg(b, p, count):
    if count != 0:
        return count, p
    return read_varint(b, p)


def unpack_pattern(b, off, plen, usize):
    out = bytearray()
    p = off
    end = off + plen
    while p < end:
        inst = b[p]
        p += 1
        op = (inst >> 5) & 0x7
        cnt = inst & 0x1F
        if op == 0:  # Zero
            n, p = read_arg(b, p, cnt)
            out += b"\x00" * n
        elif op == 1:  # blockCopy
            n, p = read_arg(b, p, cnt)
            out += b[p:p + n]
            p += n
        elif op == 2:  # repeatedBlock
            n, p = read_arg(b, p, cnt)
            rep, p = read_varint(b, p)
            block = b[p:p + n]
            p += n
            out += block * (rep + 1)
        elif op == 3:  # interleaveRepeatBlockWithBlockCopy
            common, p = read_arg(b, p, cnt)
            custom, p = read_varint(b, p)
            rep, p = read_varint(b, p)
            common_data = b[p:p + common]
            p += common
            for _ in range(rep):
                out += common_data
                out += b[p:p + custom]
                p += custom
            out += common_data
        elif op == 4:  # interleaveRepeatBlockWithZero
            common, p = read_arg(b, p, cnt)
            custom, p = read_varint(b, p)
            rep, p = read_varint(b, p)
            for _ in range(rep):
                out += b"\x00" * common
                out += b[p:p + custom]
                p += custom
            out += b"\x00" * common
        else:
            raise ValueError("reserved opcode %d at 0x%X" % (op, p - 1))
    if usize and len(out) != usize:
        raise ValueError("unpacked size mismatch %d != %d" % (len(out), usize))
    return bytes(out)


def main():
    path = sys.argv[1]
    coff = int(sys.argv[2], 0)
    plen = int(sys.argv[3], 0)
    usize = int(sys.argv[4], 0)
    data = open(path, "rb").read()
    out = unpack_pattern(data, coff, plen, usize)
    print("unpacked %d bytes" % len(out))
    for a in range(0, min(len(out), 0x40), 16):
        print("  %04X: %s" % (a, " ".join("%02X" % x for x in out[a:a + 16])))


if __name__ == "__main__":
    main()
