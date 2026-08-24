rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
print("bytes @0x15C60-0x15CD0:", rom[0x15C60:0x15CD0].hex())
