# -*- coding: utf-8 -*-
"""check_heapsym_dupe —— **多 obj 链接不得重复 `__qcc_heap_counter`**（G-D46-HEAPSYM 判据）。

现场（2026-10-24，`D:\\_objmode_test`）：
  `qcc -c a.c`（用 malloc）+ `qcc -c b.c`（用 malloc，含 main）+ `jyld a.o b.o -o x.exe`
  ⟹ `jyld: duplicate symbol '__qcc_heap_counter'`；单 obj 时 jyld 内部合并吸收 ⟹ 不撞。
  真实工程（xmalloc 分散在多个 TU）必撞。

判据：
  ① 两个 obj（都调 malloc，其一含 main）用 jyld 链接必须 rc=0；
  ② 链接产物运行退出码 = 2（b 的 main 返回值）。

用法: python scripts/check_heapsym_dupe.py [--cc qcc_zh_seed.exe]
退出码: 0 = 链接+运行都成功；1 = 重复符号/链接失败（缺陷仍在）
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

A = '空 *malloc(无符号 长 长);\n整 取甲() { 空 *p = malloc(16); 返 p ? 1 : 0; }\n'
B = '空 *malloc(无符号 长 长);\n整 取乙() { 空 *p = malloc(32); 返 p ? 2 : 0; }\n函数 整 主(空) { 返 取乙(); }\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    jyld = os.path.join(ROOT, 'srclib', 'jyld.exe')
    if not os.path.exists(cc) or not os.path.exists(jyld):
        # 2026-10-24: 缺前提 不得当绿（否则 「环境没装」会被记成「缺陷已修」）
        print('[FAIL] 环境未就绪: 缺编译器 %s 或 jyld %s' % (cc, jyld))
        return 2
    w = tempfile.mkdtemp(prefix='qcc_heapsym_')
    objs = []
    for name, text in (('a.c', A), ('b.c', B)):
        src = os.path.join(w, name)
        obj = os.path.join(w, name.replace('.c', '.o'))
        open(src, 'w', encoding='utf-8', newline='\n').write(text)
        r = subprocess.run([cc, src, '-c', '-o', obj], capture_output=True, timeout=180)
        if r.returncode != 0:
            # 2026-10-24: 试例源码编译失败 = 判据本身坏了，不能当绿
            print('[FAIL] %s 编译失败: %s' % (name, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()[:160]))
            return 1
        objs.append(obj)
    exe = os.path.join(w, 'ab.exe')
    r = subprocess.run([jyld] + objs + ['-o', exe], capture_output=True, timeout=180)
    msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()
    dupe = 'duplicate symbol' in msg and '__qcc_heap_counter' in msg
    print('[link] rc=%d dupe=%s' % (r.returncode, dupe))
    if r.returncode != 0:
        print('    ' + msg.split('\n')[0][:160])
        print('=== 多 obj heap 符号: FAIL（重复符号）===')
        return 1
    q = subprocess.run([exe], capture_output=True, timeout=60)
    ok = q.returncode == 2
    print('[run ] rc=%d (expect 2) %s' % (q.returncode, 'OK' if ok else 'WRONG'))
    print('=== 多 obj heap 符号: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
