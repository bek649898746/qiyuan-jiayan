# -*- coding: utf-8 -*-
"""跑矩阵: 每格 比较 直接调用 与 管道 的 判定。用法: python run_matrix.py <compiler.exe>"""
import io, sys, os, subprocess
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
QCC = sys.argv[1]
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'matrix2')

CASES = sorted(set(f.rsplit('_', 1)[0] for f in os.listdir(D) if f.endswith('.c')))
rows = []
for cid in CASES:
    res = {}
    for kind in ('d', 'p'):
        src = os.path.join(D, '%s_%s.c' % (cid, kind))
        exe = os.path.join(D, '%s_%s.exe' % (cid, kind))
        if os.path.exists(exe):
            os.remove(exe)
        r = subprocess.run([QCC, src, '-o', exe], capture_output=True, text=True, errors='replace')
        rc = r.returncode
        run = None
        err = (r.stdout or '') + (r.stderr or '')
        a6028 = 'A6028' in err
        if rc == 0 and os.path.exists(exe):
            rr = subprocess.run([exe], capture_output=True, text=True, errors='replace')
            run = rr.returncode
        res[kind] = (rc, a6028, run)
    d_rc, d_a, d_run = res['d']
    p_rc, p_a, p_run = res['p']
    d_ok = (d_rc == 0 and d_run == 0)
    p_ok = (p_rc == 0 and p_run == 0)
    p_rej = p_a
    verdict = 'OK' if ((d_ok and p_ok) or ((not d_ok) and p_rej)) else '**MISMATCH**'
    rows.append((cid, d_rc, d_run, 'A6028' if d_a else '', p_rc, 'A6028' if p_a else '', p_run, verdict))

print('%-16s | %-28s | %-28s | %s' % ('cell', 'direct call', 'pipe', 'verdict'))
print('-' * 100)
for cid, drc, drun, da, prc, pa, prun, v in rows:
    d = 'rc=%s run=%s %s' % (drc, drun, da)
    p = 'rc=%s %s run=%s' % (prc, pa, prun)
    print('%-16s | %-28s | %-28s | %s' % (cid, d, p, v))
bad = [r for r in rows if r[7] != 'OK']
print()
print('mismatches:', len(bad), '/', len(rows))
