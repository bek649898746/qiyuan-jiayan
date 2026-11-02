# -*- coding: utf-8 -*-
"""check_interop —— 内部缺陷追踪 混合编程两向判据（驱动真实发布编译器）。

判据：
  A. 只声明、不链接外部函数：EXE 编译必须 rc≠0 且 stderr 含 A6100，不得产出可运行的静默 0 exe；
  B. 同源 `-c` / `-S` 允许未解析符号（分模块编译不被误伤）；
  C. 正确链路：gcc 编 hot.c、qcc -c app.jy -o app.o、jyld 链接、运行两次输出 `hot_mulrow=240`；
  D. 示例 `examples/interop/` 的 README / 手册存在，手册含「必须带 `-o`」与 A6100。

用法: python scripts/check_interop.py [--cc qcc_zh_v1.exe]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import io
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run(args, cwd, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    r = subprocess.run(args, cwd=cwd, capture_output=True, env=e)
    return r.returncode, (r.stdout or b'').decode('utf-8', 'replace'), (r.stderr or b'').decode('utf-8', 'replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    qcc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    jyld = os.path.join(REPO, 'jyld.exe')
    w = tmp_dir('qcc_interop')
    bad = []

    def wfile(name, text):
        p = os.path.join(w, name)
        io.open(p, 'w', encoding='utf-8', newline='\n').write(text)
        return p

    bug = wfile('bug.jy', '整 hot_add(整 a, 整 b);\n函数 整 主(空) { 整 r = hot_add(20,22); 输出("hot_add=%d\\n", r); 返 0; }\n')
    rc, out, err = run([qcc, bug, '-o', 'bug.exe'], w)
    ok = rc != 0 and 'A6100' in err and not os.path.exists(os.path.join(w, 'bug.exe'))
    print('%s A A6100 未链接必红 | rc=%d stderr=%r' % ('PASS' if ok else 'FAIL', rc, err[:140]))
    if not ok:
        bad.append('A')

    rc, out, err = run([qcc, '-c', bug, '-o', 'bug.o'], w)
    ok = rc == 0 and os.path.exists(os.path.join(w, 'bug.o'))
    print('%s B1 -c 允许未解析 | rc=%d' % ('PASS' if ok else 'FAIL', rc))
    if not ok:
        bad.append('B1')

    rc, out, err = run([qcc, '-S', bug, '-o', 'bug.s'], w)
    ok = rc == 0
    print('%s B2 -S 允许未解析 | rc=%d' % ('PASS' if ok else 'FAIL', rc))
    if not ok:
        bad.append('B2')

    wfile('hot.c', 'int hot_add(int a, int b) { return a + b + 1000; }\n'
                   'int hot_mulrow(int *a, int *b, int n) { int s=0; for(int k=0;k<n;k++) s+=a[k]*b[k]; return s; }\n')
    rc, _, err = run(['gcc', '-O2', '-c', 'hot.c', '-o', 'hot.o'], w)
    if rc != 0:
        print('FAIL C0 gcc 编 hot.o | %r' % err[:120])
        bad.append('C0')

    # C1: 同一 hot_add 程序 + 正确链接 hot.o -> 运行两次 hot_add=1042（20+22+1000）
    rc, _, err = run([jyld, 'bug.o', 'hot.o', '-o', 'bug_linked.exe'], w)
    if rc != 0:
        print('FAIL C1 jyld 链接 bug | %r' % err[:160])
        bad.append('C1')
    rc1, o1, _ = run([os.path.join(w, 'bug_linked.exe')], w)
    rc2, o2, _ = run([os.path.join(w, 'bug_linked.exe')], w)
    ok = rc1 == 0 and rc2 == 0 and 'hot_add=1042' in o1 and 'hot_add=1042' in o2
    print('%s C1 链接后两次 hot_add=1042 | %r / %r' % ('PASS' if ok else 'FAIL', o1.strip(), o2.strip()))
    if not ok:
        bad.append('C1')
    app = wfile('app.jy', '整 hot_mulrow(整 *a, 整 *b, 整 n);\n函数 整 主(空) {\n    整 甲[8] = {1,2,3,4,5,6,7,8};\n    整 乙[8] = {2,3,4,5,6,7,8,9};\n    输出("hot_mulrow=%d\\n", hot_mulrow(甲, 乙, 8));\n    返 0;\n}\n')
    rc, _, err = run([qcc, '-c', app, '-o', 'app.o'], w)
    if rc != 0:
        print('FAIL C2 qcc -c app.jy | %r' % err[:120])
        bad.append('C2')
    rc, _, err = run([jyld, 'app.o', 'hot.o', '-o', 'app.exe'], w)
    if rc != 0:
        print('FAIL C3 jyld 链接 | %r' % err[:160])
        bad.append('C3')
    rc1, o1, _ = run([os.path.join(w, 'app.exe')], w)
    rc2, o2, _ = run([os.path.join(w, 'app.exe')], w)
    ok = rc1 == 0 and rc2 == 0 and 'hot_mulrow=240' in o1 and 'hot_mulrow=240' in o2
    print('%s C4 运行两次 240 | %r / %r' % ('PASS' if ok else 'FAIL', o1.strip(), o2.strip()))
    if not ok:
        bad.append('C4')

    readme = os.path.join(REPO, 'examples', 'interop', 'README.md')
    manual = os.path.join(REPO, 'manual', '混合编程.md')
    ok_doc = os.path.exists(readme) and os.path.exists(manual)
    ok_manual = ok_doc and '必须带' in io.open(manual, encoding='utf-8').read() and 'A6100' in io.open(manual, encoding='utf-8').read()
    print('%s D 示例/手册 | readme=%s manual=%s' % ('PASS' if ok_doc and ok_manual else 'FAIL', os.path.exists(readme), os.path.exists(manual)))
    if not (ok_doc and ok_manual):
        bad.append('D')

    shutil.rmtree(w, ignore_errors=True)
    print('== 混合编程判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
