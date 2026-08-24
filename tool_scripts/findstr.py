import pathlib
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
s = b'VMMaxVirtualPages'
i = d.find(s)
print('rom_flat offset', hex(i))
print(d[i-16:i+80])
p = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\mac_os_rom.bin').read_bytes()
i = p.find(s)
print('mac_os_rom offset', hex(i))
print(p[i-16:i+80])
