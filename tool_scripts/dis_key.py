import struct
f = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat.bin","rb")
def rd(off):
    f.seek(off)
    return struct.unpack(">I", f.read(4))[0]
def op(ins):
    import re
    opc = ins >> 26
    names = {3:"twi",4:"tw",7:"mulli",14:"addi",15:"addis",18:"b",19:"x",20:"rlwimi",24:"ori",25:"oris",26:"xori",28:"andi.",31:"x",32:"lwz",33:"lwzu",34:"lbz",35:"lbzu",36:"stw",37:"stwu",38:"stb",40:"lhz",42:"lha",44:"sth",48:"lfs",50:"lfd",54:"stfd",60:"fpu"}
    n = names.get(opc, f"op{opc}")
    return f"{n:5s} 0x{ins:08x}"
# NK base = 0x40800000 -> file offset = addr - 0x40800000
for addr in [0x40B14700, 0x40B24524, 0x40B13BF8, 0x40B24500, 0x40B14400]:
    off = addr - 0x40800000
    print(f"==== 0x{addr:08x} (file 0x{off:x}) ====")
    for i in range(0, 8):
        w = rd(off + i*4)
        print(f"  0x{addr+i*4:08x}: {op(w)}")
