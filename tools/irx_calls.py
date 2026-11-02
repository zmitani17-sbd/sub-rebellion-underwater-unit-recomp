# irx_calls.py <irx> <library> <ordinal> [context]: list call sites of an imported function in an IRX
import capstone, struct, subprocess, sys
fn, lib, ordinal = sys.argv[1], sys.argv[2], int(sys.argv[3])
ctx = int(sys.argv[4]) if len(sys.argv) > 4 else 16
d = open(fn, 'rb').read()
out = subprocess.run(['readelf', '-S', '-W', fn], capture_output=True, text=True).stdout
for line in out.splitlines():
    if ' .text ' in line:
        parts = line.split(']')[1].split(); toff = int(parts[3], 16); tsize = int(parts[4], 16)
code = d[toff:toff + tsize]
i = 0; stubs = {}
while i < tsize - 4:
    if struct.unpack_from('<I', code, i)[0] == 0x41e00000:
        name = code[i + 12:i + 20].rstrip(b'\0').decode(errors='replace'); j = i + 20
        while j < tsize - 8:
            a, b = struct.unpack_from('<II', code, j)
            if a == 0x03e00008 and (b >> 16) == 0x2400: stubs[j] = (name, b & 0xFFFF); j += 8
            else: break
        i = j
    else: i += 4
md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS32 + capstone.CS_MODE_LITTLE_ENDIAN)
insns = list(md.disasm(code, 0)); by = {x.address: x for x in insns}
targets = [k for k, (n, o) in stubs.items() if n == lib and o == ordinal]
def label(ins):
    if ins.mnemonic in ('jal', 'j'):
        t = int(ins.op_str, 16)
        if t in stubs: return '   <%s %d>' % stubs[t]
    return ''
for x in insns:
    if x.mnemonic == 'jal' and int(x.op_str, 16) in targets:
        print('--- %s: call to %s %d at +%x' % (fn, lib, ordinal, x.address))
        for k in range(x.address - ctx * 4, x.address + ctx * 4, 4):
            y = by.get(k)
            if y: print('   +%04x %-7s %s%s' % (y.address, y.mnemonic, y.op_str, label(y)))
