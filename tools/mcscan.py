import sys, struct
PAGE, RAW = 512, 528
for path in sys.argv[1:]:
    raw = open(path, 'rb').read()
    # strip ECC spare bytes
    data = b''.join(raw[i:i+PAGE] for i in range(0, len(raw), RAW))
    sb = data[:0x154]
    print('==', path, sb[:28])
    found = set()
    # directory entries are 512 bytes, name at 0x40; scan each page
    for off in range(0, len(data), PAGE):
        e = data[off:off+PAGE]
        mode, = struct.unpack_from('<H', e, 0)
        name = e[0x40:0x60].split(b'\0')[0]
        if (mode & 0x8000) and (mode & 0x20) and name and all(32 <= c < 127 for c in name) and name not in (b'.', b'..'):
            length, = struct.unpack_from('<I', e, 4)
            # modified time at 0x18: sec,min,hour,day,month,year(u16)
            _, s, m, h, d, mo, y = struct.unpack_from('<BBBBBBH', e, 0x18)
            key = (name, length)
            if key in found: continue
            found.add(key)
            print(f'  dir {name.decode():<32} entries={length:<4} modified={y}-{mo:02}-{d:02} {h:02}:{m:02}')
