# -*- coding: utf-8 -*-
"""D-SPLIT 拆单元 ABI 判据（2026-10-18）。

判据：
  ① `qcc -c a.c` / `qcc -c b.c` 各自 rc=0；
  ② 含 `主` 的入口对象 `nm` 必须见 `T main`，且**不见** `主`（-c 对象模式重命名，避免别名自递归）；
  ③ 不含 `主` 的库对象 `nm` **不得**出现 `main`（不污染库对象）；
  ④ `gcc -r` + `gcc` 链接两个对象 ⟹ 可执行；运行 rc=42；
  ⑤ 反例：只链库对象 ⟹ 链接失败（缺入口），证明入口来自被编译对象。
用法: python scripts/check_split_unit.py --cc qcc_zh_v2.exe
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

A = '函数 整 加(整 a, 整 b) { 返 a + b; }\n'
B = '函数 整 加(整 a, 整 b);\n函数 整 主(空) { 返 加(20, 22); }\n'


def run(args, timeout=180):
    return subprocess.run(args, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', timeout=timeout, cwd=REPO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    a = ap.parse_args()
    w = tempfile.mkdtemp(prefix='qcc_split_')
    fails = []
    pa, pb = os.path.join(w, 'sa.c'), os.path.join(w, 'sb.c')
    oa, ob = os.path.join(w, 'sa.o'), os.path.join(w, 'sb.o')
    open(pa, 'w', encoding='utf-8', newline='\n').write(A)
    open(pb, 'w', encoding='utf-8', newline='\n').write(B)

    for src, obj in ((pa, oa), (pb, ob)):
        r = run([a.cc, '-c', src, '-o', obj])
        if r.returncode != 0:
            fails.append('-c 失败 %s rc=%d %s%s' % (os.path.basename(src), r.returncode, r.stdout, r.stderr))
    nm = shutil.which('nm') or 'nm'
    na = run([nm, oa]).stdout if os.path.exists(oa) else ''
    nb = run([nm, ob]).stdout if os.path.exists(ob) else ''
    if 'T main' not in nb:
        fails.append('入口对象 nm 缺 T main')
    if 'T 主' in nb or ' T 主' in nb:
        fails.append('入口对象仍见 T 主（-c 未重命名，会与 main 别名冲突）')
    if 'main' in na:
        fails.append('库对象被污染：出现 main 符号')

    ab = os.path.join(w, 'ab.o')
    r = run(['gcc', '-r', oa, ob, '-o', ab])
    if r.returncode != 0:
        fails.append('gcc -r 失败: %s%s' % (r.stdout, r.stderr))
    exe = os.path.join(w, 'ab.exe')
    r = run(['gcc', ab, '-o', exe])
    if r.returncode != 0:
        fails.append('链接 exe 失败: %s%s' % (r.stdout, r.stderr))
    else:
        q = run([exe])
        if q.returncode != 42:
            fails.append('运行 rc=%d（应 42）out=%r' % (q.returncode, q.stdout))

    # 反例：只有库对象，链接必须失败（缺入口）
    r = run(['gcc', oa, '-o', os.path.join(w, 'noentry.exe')])
    if r.returncode == 0:
        fails.append('反例：只链库对象却成功（入口来源不可信）')

    print('== D-SPLIT 拆单元 ABI ==')
    print('objects: %s' % ('OK' if not any('-c' in f or 'nm' in f or '污染' in f for f in fails) else 'FAIL'))
    print('link/run: %s' % ('OK' if not any('gcc' in f or '运行' in f for f in fails) else 'FAIL'))
    print('negative(no entry): %s' % ('OK' if not any('反例' in f for f in fails) else 'FAIL'))
    if fails:
        print('FAIL=%d' % len(fails))
        for f in fails:
            print('  - ' + f)
        print('work=' + w)
        return 1
    print('=== D-SPLIT split unit: PASS ===')
    shutil.rmtree(w, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
