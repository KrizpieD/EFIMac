import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
n = len(rom)//4
words = struct.unpack(f">{n}I", rom[:n*4])
lo, hi = 0x40B60000, 0x40BD0000
runs=[]
i=0
while i < n-4:
    if lo <= words[i] <= hi:
        j=i
        while j<n and lo<=words[j]<=hi: j+=1
        if j-i >= 24:
            runs.append((i,j-i))
        i=j
    else:
        i+=1
for start,length in sorted(runs,key=lambda t:-t[1])[:10]:
    print(f"@ROM+{start*4:06x} ({base+start*4:#x}) len={length}")
    print("   ", " ".join(f"{words[start+k]:08x}" for k in range(min(10,length))))
