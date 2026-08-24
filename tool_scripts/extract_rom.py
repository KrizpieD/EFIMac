import sys
sys.path.insert(0, r"C:\Users\clayc\Desktop\New folder (2)\EFIMac\tools")
import hfs_read

img = hfs_read.Image(r"C:\Users\clayc\AppData\Local\Temp\opencode\mac_disc\Mac_OS_9.2.2.iso")
vol, err = hfs_read.mount(img)
print("mount:", vol.name if vol else err)
if vol is None:
    sys.exit(1)
kids = vol.children_of(2)
print("root children:", len(kids))
for k in kids:
    print("  ", k)
# find the Mac OS ROM under System Folder
def find(cid, prefix=""):
    for kid in vol.children_of(cid):
        name = kid.get("name", "?")
        if "dnum" in kid:
            r = find(kid["dnum"], prefix + name + ":")
            if r: return r
        else:
            if name.lower() == "mac os rom":
                return (prefix + name, kid)
    return None
r = find(2)
print("MACOSROM:", r)
