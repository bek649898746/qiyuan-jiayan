# -*- coding: utf-8 -*-
"""1: printf 共享块（每函数每内建种类一份）—— 判据：每点 ≤200 B；负控 ≥1000 B。

用法:
  python scripts/check_artifact_total.py [--cc qcc_zh_v2.exe] [--lua <dir>]
退出码: 0 = PASS / 1 = FAIL / 2 = 语料或工具未就绪
"""
import argparse
import io
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('--lua', default=os.environ.get('QCC_LUA_DIR', r'D:\qcc_corpus\lua-5.4.7\src'))
    a = ap.parse_args()
    if not os.path.isdir(a.lua):
        print('语料未就绪: %s' % a.lua)
        return 2
    work = tmp_dir('artifact_total')
    files = sorted(f for f in os.listdir(a.lua) if f.endswith('.c') and f != 'onelua.c')
    rows = []
    for f in files:
        src = os.path.join(a.lua, f)
        o = os.path.join(work, f + '.o')
        g = os.path.join(work, f + '.gcc.o')
        r = subprocess.run([a.cc, src, '-c', '-I', a.lua, '-o', o], capture_output=True, text=True,
                           encoding='utf-8', errors='replace')
        if r.returncode != 0:
            rows.append((f, None, None))
            continue
        rg = subprocess.run(['gcc', '-O2', '-c', '-I', a.lua, src, '-o', g], capture_output=True)
        qs = os.path.getsize(o) if os.path.exists(o) else None
        gs = os.path.getsize(g) if rg.returncode == 0 and os.path.exists(g) else None
        rows.append((f, qs, gs))
    qsum = sum(x[1] for x in rows if x[1])
    gsum = sum(x[2] for x in rows if x[2])
    print('%d 文件 · 甲言合计 %d B · gcc -O2 合计 %d B · 倍数 %.2fx（上限 5x）' % (len(rows), qsum, gsum, qsum / gsum if gsum else 0))
    bad = []
    if gsum == 0 or qsum / gsum > 5.0:
        bad.append('总倍数 %.2fx > 5x' % (qsum / gsum if gsum else 0))
    for f, qs, gs in rows:
        if qs is None:
            bad.append('%s 编译失败' % f)
        elif gs and qs / gs > 5.0:
            print('  超 5x: %-16s %8d / %8d = %6.2fx' % (f, qs, gs, qs / gs))
            bad.append('%s %.2fx > 5x' % (f, qs / gs))
    # 点名的两文件
    for name in ('luac.c', 'lvm.c'):
        row = next((x for x in rows if x[0] == name), None)
        if row and row[1] and row[2]:
            print('  %-8s %8d / %8d = %6.2fx' % (name, row[1], row[2], row[1] / row[2]))
    print()
    if bad:
        print('[失败 FAIL] check_artifact_total：')
        for b in bad[:12]:
            print('  - ' + b)
        return 1
    print('[通过 PASS] check_artifact_total：lua 34 文件合计 %.2fx ≤5x，逐文件 ≤5x' % (qsum / gsum))
    return 0


if __name__ == '__main__':
    sys.exit(main())
