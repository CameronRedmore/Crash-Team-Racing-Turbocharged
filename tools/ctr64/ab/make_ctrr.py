# Build a synthetic V3 .ctrr from retail BIGFILE subfiles (test-only).
import struct, sys
big = open(sys.argv[1], 'rb')
out, name, model_char, template = sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
def sub(i):
    big.seek(0); cdpos, n = struct.unpack('<ii', big.read(8))
    big.seek(8 + 8 * i); off, size = struct.unpack('<ii', big.read(8))
    big.seek(off << 11); return big.read(size)
model = sub(242 + model_char)     # BI_RACERMODELHI
vrm = sub(258)                     # BI_SHAREDMPKVRM
HDR = 596
assets = [(HDR, len(model)), (HDR + len(model), len(vrm)), (0, 0), (0, 0)]
h = struct.pack('<IHHhhI', 0x52525443, 3, HDR, template, -1, 0) + bytes(32)
h += name.encode().ljust(64, b'\0') + b'ctr64 test'.ljust(64, b'\0')
h += b''.join(struct.pack('<II', o, s) for o, s in assets) + struct.pack('<I', 0) + bytes(32 * 12)
assert len(h) == HDR, len(h)
open(out, 'wb').write(h + model + vrm)
print(out, len(model), len(vrm))
