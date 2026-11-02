import sys, struct, os
# Raw Mode 2 Form 1 CD image (2352-byte sectors, user data at +24) -> walk ISO9660, list and extract.
img, outdir = sys.argv[1], sys.argv[2]
extract = sys.argv[3:]  # name patterns (upper-case suffix/names) to extract; '*' = all
f = open(img, 'rb')
def sector(lba, n=1):
    out = bytearray()
    for i in range(n):
        f.seek((lba + i) * 2352)
        s = f.read(2352)
        form2 = s[18] & 0x20
        out += s[24:24 + (2324 if form2 else 2048)]
    return bytes(out)
def read_file(lba, size):
    out = bytearray()
    n = (size + 2047) // 2048
    for i in range(n):
        out += sector(lba + i)[:2048]
    return bytes(out[:size])
pvd = sector(16)
assert pvd[1:6] == b'CD001'
print('Volume:', pvd[40:72].decode(errors='replace').strip())
root = pvd[156:156 + 34]
files = []
def walk(lba, size, path):
    data = read_file(lba, size)
    i = 0
    while i < len(data):
        l = data[i]
        if l == 0:
            i = ((i // 2048) + 1) * 2048
            continue
        rec = data[i:i + l]
        elba = struct.unpack_from('<I', rec, 2)[0]
        esize = struct.unpack_from('<I', rec, 10)[0]
        flags = rec[25]
        nl = rec[32]
        name = rec[33:33 + nl]
        i += l
        if name in (b'\x00', b'\x01'):
            continue
        name = name.decode(errors='replace').split(';')[0]
        p = path + '/' + name
        if flags & 2:
            walk(elba, esize, p)
        else:
            files.append((p, elba, esize))
walk(struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0], '')
for p, lba, size in sorted(files, key=lambda x: x[1]):
    print(f'{lba:8d} {size:12d} {p}')
os.makedirs(outdir, exist_ok=True)
for p, lba, size in files:
    up = p.upper()
    if '*' in extract or any(up.endswith(e) or os.path.basename(up) == e for e in extract):
        dst = os.path.join(outdir, p.lstrip('/'))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, 'wb').write(read_file(lba, size))
print('total files', len(files), 'bytes', sum(s for _, _, s in files))
