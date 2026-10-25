# -*- coding: utf-8 -*-
"""check_link_mode_size —— **单文件模式产物不得把整份运行库全量编入**（G-OBJ-SIZE 判据）。

病灶（2026-10-24 现场，`D:\\_objmode_test`）：
  `k0..k7` 八个最小程序产物全 **287,232 B**（printf 版 295,424 B）；
  `-c` 空 main 仅 334 B；`-c` + jyld 按需库 5,120 B（−98.2%，运行 exit=0）。
  ⟹ 单文件模式把 153 个运行库函数全量内联，即使程序一个都不用。

判据：最小 `格式化` 程序 + 最小 `返 0` 程序，产物都必须 ≤ 200,000 B（现 283–295 KB ⟹ 红）。
      （阈值取 200 KB：给运行库必需部分留足余量，同时远小于「全量 153 函数」的 287 KB。）

用法: python scripts/check_link_mode_size.py [--cc qcc_zh_seed.exe]
退出码: 0 = 两个产物都 ≤ 阈值；1 = 超阈值（缺陷仍在）
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIMIT = 200000

CASES = [
    ('bare', '函数 整 主(空) { 返 0; }\n'),
    ('printf', '#包含 <stdio.h>\n函数 整 主(空) { 格式化 "hi\\n"; 返 0; }\n'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        # 2026-10-24（判据方打回）: 缺主体不得当绿 —— rc=2 = 环境未就绪，不是 PASS
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_size_')
    bad = []
    for name, text in CASES:
        src = os.path.join(w, name + '.c')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(text)
        exe = os.path.join(w, name + '.exe')
        r = subprocess.run([cc, src, '-o', exe], capture_output=True, timeout=300)
        n = os.path.getsize(exe) if r.returncode == 0 and os.path.exists(exe) else -1
        ok = n > 0 and n <= LIMIT
        print('[%-7s] rc=%d size=%d (limit %d) %s' % (name, r.returncode, n, LIMIT, 'OK' if ok else 'TOO-BIG'))
        if not ok:
            bad.append(name)
    print('=== 单文件模式产物体积: %s ===' % ('PASS' if not bad else 'FAIL'))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
