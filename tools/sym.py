import subprocess, sys, bisect
out = subprocess.run(['readelf', '-s', '-W', sys.argv[1]], capture_output=True, text=True).stdout
fn = sorted((int(p[1], 16), int(p[2]), p[7]) for p in (l.split() for l in out.splitlines()) if len(p) >= 8 and p[3] == 'FUNC')
addrs = [a for a, _, _ in fn]
for q in sys.argv[2:]:
    q = int(q, 16); i = bisect.bisect_right(addrs, q) - 1
    a, s, n = fn[i]; print(f'{q:08x} -> {n} +0x{q-a:x}')
