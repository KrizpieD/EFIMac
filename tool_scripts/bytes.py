rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
print("bytes @0x815BE0-0x815CB0:", rom[0x815BE0:0x815CB0].hex())
