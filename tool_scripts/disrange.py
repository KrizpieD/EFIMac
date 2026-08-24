import sys
sys.argv = ["x", "0x40800000", "0x40800004"]
import importlib.util
spec = importlib.util.spec_from_file_location("ppc_dis", r"C:\Users\clayc\AppData\Local\Temp\opencode\ppc_dis.py")
# monkeypatch main so import doesn't try to parse argv
import types
P = importlib.util.module_from_spec(spec)
sys.modules["ppc_dis"] = P
P.main = lambda: None
spec.loader.exec_module(P)

data = P.data

def dis(addr, n=16):
    off = addr - 0x40800000
    for i in range(n):
        w = int.from_bytes(data[off:off+4], "big")
        print("0x%08X: %08X  %s" % (addr + i*4, w, P.dis(w, addr + i*4)))
        off += 4

if __name__ == "__main__":
    start = int(sys.argv[1], 0)
    n = int(sys.argv[2], 0)
    dis(start, n)
