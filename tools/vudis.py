#!/usr/bin/env python3
"""Minimal PS2 VU1 microcode disassembler (lower + upper), enough to follow control flow.

  python3 tools/vudis.py vu1code.bin <start-hex> [count]

vu1code.bin is a 16 KB VU1 micro memory image. For Sub Rebellion, build it from the ELF with
`tools/vudis.py --build SLUS_205.48 vu1code.bin`: the game's .vutext holds eight MPG uploads
(0x800 bytes each, the last 0x438), each after an 8-byte "NOP; MPG" VIFcode pair.
"""
import struct
import sys

FIELD = ['x', 'y', 'z', 'w']


def dest(i):
    d = (i >> 21) & 0xF
    return ''.join(c for b, c in zip((8, 4, 2, 1), 'xyzw') if d & b)


def s11(v):
    v &= 0x7FF
    return v - 0x800 if v & 0x400 else v


def s5(v):
    v &= 0x1F
    return v - 0x20 if v & 0x10 else v


def lower(i, pc):
    op = (i >> 25) & 0x7F
    it, is_, id_ = (i >> 16) & 0x1F, (i >> 11) & 0x1F, (i >> 6) & 0x1F
    ft, fs, fd = it, is_, id_
    imm11 = s11(i)
    imm15 = (((i >> 21) & 0xF) << 11) | (i & 0x7FF)
    tgt = lambda: '0x%x' % ((pc + 8 + imm11 * 8) & 0x3FFF)
    simple = {
        0x00: lambda: 'LQ.%s vf%d, %d(vi%d)' % (dest(i), ft, imm11, is_),
        0x01: lambda: 'SQ.%s vf%d, %d(vi%d)' % (dest(i), fs, imm11, it),
        0x04: lambda: 'ILW.%s vi%d, %d(vi%d)' % (dest(i), it, imm11, is_),
        0x05: lambda: 'ISW.%s vi%d, %d(vi%d)' % (dest(i), it, imm11, is_),
        0x08: lambda: 'IADDIU vi%d, vi%d, 0x%x' % (it, is_, imm15),
        0x09: lambda: 'ISUBIU vi%d, vi%d, 0x%x' % (it, is_, imm15),
        0x10: lambda: 'FCEQ vi1, 0x%x' % (i & 0xFFFFFF),
        0x11: lambda: 'FCSET 0x%x' % (i & 0xFFFFFF),
        0x12: lambda: 'FCAND vi1, 0x%x' % (i & 0xFFFFFF),
        0x13: lambda: 'FCOR vi1, 0x%x' % (i & 0xFFFFFF),
        0x14: lambda: 'FSEQ vi%d, 0x%x' % (it, ((i >> 10) & 0x800) | (i & 0x7FF)),
        0x15: lambda: 'FSSET 0x%x' % (((i >> 10) & 0x800) | (i & 0x7FF)),
        0x16: lambda: 'FSAND vi%d, 0x%x' % (it, ((i >> 10) & 0x800) | (i & 0x7FF)),
        0x17: lambda: 'FSOR vi%d, 0x%x' % (it, ((i >> 10) & 0x800) | (i & 0x7FF)),
        0x18: lambda: 'FMEQ vi%d, vi%d' % (it, is_),
        0x1A: lambda: 'FMAND vi%d, vi%d' % (it, is_),
        0x1B: lambda: 'FMOR vi%d, vi%d' % (it, is_),
        0x1C: lambda: 'FCGET vi%d' % it,
        0x20: lambda: 'B %s' % tgt(),
        0x21: lambda: 'BAL vi%d, %s' % (it, tgt()),
        0x24: lambda: 'JR vi%d' % is_,
        0x25: lambda: 'JALR vi%d, vi%d' % (it, is_),
        0x28: lambda: 'IBEQ vi%d, vi%d, %s' % (it, is_, tgt()),
        0x29: lambda: 'IBNE vi%d, vi%d, %s' % (it, is_, tgt()),
        0x2C: lambda: 'IBLTZ vi%d, %s' % (is_, tgt()),
        0x2D: lambda: 'IBGTZ vi%d, %s' % (is_, tgt()),
        0x2E: lambda: 'IBLEZ vi%d, %s' % (is_, tgt()),
        0x2F: lambda: 'IBGEZ vi%d, %s' % (is_, tgt()),
    }
    if op in simple:
        return simple[op]()
    if op != 0x40:
        return 'lower?%02x %08x' % (op, i)
    func = i & 0x3F
    if func == 0x30:
        return 'IADD vi%d, vi%d, vi%d' % (id_, is_, it)
    if func == 0x31:
        return 'ISUB vi%d, vi%d, vi%d' % (id_, is_, it)
    if func == 0x32:
        return 'IADDI vi%d, vi%d, %d' % (it, is_, s5(i >> 6))
    if func == 0x34:
        return 'IAND vi%d, vi%d, vi%d' % (id_, is_, it)
    if func == 0x35:
        return 'IOR vi%d, vi%d, vi%d' % (id_, is_, it)
    if func < 0x3C:
        return 'lower?40/%02x %08x' % (func, i)
    sp = (i & 3) | ((i >> 4) & 0x7C)
    fsf, ftf = FIELD[(i >> 21) & 3], FIELD[(i >> 23) & 3]
    table = {
        0x30: 'MOVE.%s vf%d, vf%d' % (dest(i), ft, fs),
        0x31: 'MR32.%s vf%d, vf%d' % (dest(i), ft, fs),
        0x34: 'LQI.%s vf%d, (vi%d++)' % (dest(i), ft, is_),
        0x35: 'SQI.%s vf%d, (vi%d++)' % (dest(i), fs, it),
        0x36: 'LQD.%s vf%d, (--vi%d)' % (dest(i), ft, is_),
        0x37: 'SQD.%s vf%d, (--vi%d)' % (dest(i), fs, it),
        0x38: 'DIV Q, vf%d%s, vf%d%s' % (fs, fsf, ft, ftf),
        0x39: 'SQRT Q, vf%d%s' % (ft, ftf),
        0x3A: 'RSQRT Q, vf%d%s, vf%d%s' % (fs, fsf, ft, ftf),
        0x3B: 'WAITQ',
        0x3C: 'MTIR vi%d, vf%d%s' % (it, fs, fsf),
        0x3D: 'MFIR.%s vf%d, vi%d' % (dest(i), ft, is_),
        0x3E: 'ILWR.%s vi%d, (vi%d)' % (dest(i), it, is_),
        0x3F: 'ISWR.%s vi%d, (vi%d)' % (dest(i), it, is_),
        0x40: 'RNEXT', 0x41: 'RGET', 0x42: 'RINIT', 0x43: 'RXOR',
        0x64: 'MFP.%s vf%d, P' % (dest(i), ft),
        0x68: 'XTOP vi%d' % it,
        0x69: 'XITOP vi%d' % it,
        0x6C: 'XGKICK vi%d' % is_,
        0x7B: 'WAITP',
    }
    if sp in table:
        return table[sp]
    if 0x70 <= sp <= 0x7F:
        return 'EFU%02x vf%d' % (sp, fs)
    return 'lower?sp%02x %08x' % (sp, i)


UPPER3 = {0x00: 'ADD', 0x01: 'SUB', 0x02: 'MADD', 0x03: 'MSUB', 0x04: 'MAX', 0x05: 'MINI',
          0x06: 'MUL', 0x07: 'MULq'}


def upper(i):
    flags = ''.join(c for b, c in ((31, 'I'), (30, 'E'), (29, 'M'), (28, 'D'), (27, 'T')) if (i >> b) & 1)
    if (i & 0x7FF) == 0x2FF:
        return 'NOP', flags
    func = i & 0x3F
    names = {0x1C: 'MULq', 0x1E: 'MULi', 0x1F: 'MINIi', 0x20: 'ADDq', 0x21: 'MADDq', 0x22: 'ADDi',
             0x23: 'MADDi', 0x24: 'SUBq', 0x25: 'MSUBq', 0x26: 'SUBi', 0x27: 'MSUBi', 0x28: 'ADD',
             0x29: 'MADD', 0x2A: 'MUL', 0x2B: 'MAX', 0x2C: 'SUB', 0x2D: 'MSUB', 0x2E: 'OPMSUB',
             0x2F: 'MINI'}
    if func < 0x1C:
        n = ['ADD', 'SUB', 'MADD', 'MSUB', 'MAX', 'MINI', 'MUL'][func >> 2] if func < 0x1C else '?'
        n += 'xyzw'[func & 3]
    elif func in names:
        n = names[func]
    elif func >= 0x3C:
        n = 'ACC/conv.%03x' % (((i >> 4) & 0x7C) | (i & 3))
    else:
        n = 'up?%02x' % func
    return '%s.%s vf%d, vf%d, vf%d' % (n, dest(i), (i >> 6) & 0x1F, (i >> 11) & 0x1F, (i >> 16) & 0x1F), flags


def build(elf, out):
    d = open(elf, 'rb').read()
    # .vutext file offset from the section table
    e_shoff, = struct.unpack_from('<I', d, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', d, 0x2E)
    shstr_off = struct.unpack_from('<I', d, e_shoff + e_shstrndx * e_shentsize + 16)[0]
    base = None
    for k in range(e_shnum):
        sh = e_shoff + k * e_shentsize
        name_off, = struct.unpack_from('<I', d, sh)
        name = d[shstr_off + name_off:d.index(b'\0', shstr_off + name_off)].decode()
        if name == '.vutext':
            base = struct.unpack_from('<I', d, sh + 16)[0]
    code = bytearray(0x4000)
    for k in range(8):
        off = base + 0xE0 + k * 0x808
        size = 0x800 if k < 7 else 0x438
        code[k * 0x800:k * 0x800 + size] = d[off:off + size]
    open(out, 'wb').write(code)


def main():
    if sys.argv[1] == '--build':
        build(sys.argv[2], sys.argv[3])
        return
    code = open(sys.argv[1], 'rb').read()
    start = int(sys.argv[2], 16)
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 32
    for pc in range(start, min(start + count * 8, len(code)), 8):
        lo, up = struct.unpack_from('<II', code, pc)
        u, flags = upper(up)
        l = 'LOI 0x%08x' % lo if (up >> 31) & 1 else lower(lo, pc)
        print('%04x: %-34s | %-28s %s' % (pc, u, l, flags))


if __name__ == '__main__':
    main()
