# -*- coding: utf-8 -*-
"""check_long_width_matrix — `long` 家族宽度全矩阵（ORD-20261030-63 收口判据）。

覆盖 ORD-63 剩余三类副本的**行为面**（不再靠源码逐行数）：
  A `sizeof`：char=1 / short=2 / int=4 / **long=4**（LLP64）/ **long long=8** / 指针=8
  B 结构字段布局：`构 { long; long long; int; }` ⟹ 尺寸 24 · b 偏移 8 · c 偏移 16
  C 形参传递：`函数 长 f(long)` / `长 f(long long)` 取值不变

用法: python scripts/check_long_width_matrix.py [--cc qcc_zh_v1.exe]
退出码：0 = 全对；1 = 有偏。
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(REPO, 'scratch_long_width')

SRC = '''# 包含 <stdio.h>
构 甲 { long a; long long b; int c; };
函数 长 a取(long v) { 返 v; }
函数 长 b取(long long v) { 返 v; }
函数 整 主(空){
    格式化 "SZ %d %d %d %d %d %d\\n",
        (整)大小(char), (整)大小(short), (整)大小(int), (整)大小(long), (整)大小(long long), (整)大小(void*);
    格式化 "LAY %d %d %d\\n", (整)大小(构 甲),
        (整)((字*)(&((构 甲*)0)->b)), (整)((字*)(&((构 甲*)0)->c));
    格式化 "PAR %d %d\\n", (整)a取(7), (整)b取(9);
    返 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(WORK, 'lw.jy')
    exe = os.path.join(WORK, 'lw.exe')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(SRC)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    if r.returncode != 0:
        print('[编译] rc=%d : %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:200]))
        return 1
    q = subprocess.run([exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=60)
    lines = [l.strip() for l in (q.stdout or '').split('\n') if l.strip()]
    print('[run] ' + ' / '.join(lines))
    exp = {
        'SZ': 'SZ 1 2 4 4 8 8',
        'LAY': 'LAY 24 8 16',
        'PAR': 'PAR 7 9',
    }
    fails = []
    seen = {}
    for l in lines:
        k = l.split()[0] if l.split() else ''
        seen[k] = l
    for k, e in exp.items():
        got = seen.get(k, '<缺失>')
        ok = (got == e)
        print('[%s] %r 期望 %r : %s' % (k, got, e, 'PASS' if ok else 'FAIL'))
        if not ok:
            fails.append(k)
    if fails:
        return 1
    print('[PASS] long 家族宽度矩阵全对（sizeof / 字段布局 / 形参）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
