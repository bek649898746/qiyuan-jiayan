#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""STATIC-SILENT 判据 —— 编译器自身静态足迹（`SizeOfImage`）必须低于阈值，且**按需补丁表**真能用。

## 判据（两条都要过）

1. **足迹**：编译器 PE 的 `SizeOfImage` ≤ 1270 MiB。
   由来：`全败[1048576]`（1M×12B = 12MB）改按需后，改前 1273.1MB → 改后 1261.1MB；
   阈值 1270 落在两者之间 ⟹ 改前红、改后绿（同一命令同一门槛）。
2. **功能**：生成一个含 **300 段前向分支** 的程序，编译 + 运行必须输出 300。
   这直接驱动 `补标号()` / `解析补丁()` —— 按需表若没被正确分配/回填，这里必错或崩。

用法：
    python scripts/check_static_footprint.py            # 默认 qcc_zh_v1.exe
    python scripts/check_static_footprint.py --cc qcc_pd.exe   # 改前 ⟹ 足迹一条红
退出码：0 = 两条全过；1 = 有红；2 = 环境缺件。
"""
import argparse
import os
import struct
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_static')
LIMIT_MB = 640
LABELS = 300


def size_of_image(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    assert d[pe:pe + 4] == b'PE\0\0'
    return struct.unpack_from('<I', d, pe + 24 + 56)[0]


def gen_labels(n):
    L = ['# 包含 <stdio.h>', '函数 整 主(空) {', '    整 和 = 0;']
    for i in range(n):
        L.append('    若 (%d < %d) { 和 = 和 + 1; }' % (i, n))
    L.append('    格式化 "LBL %d\\n", 和;')
    L += ['    返 (和 == %d) ? 0 : 1;' % n, '}']
    return '\n'.join(L) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    args = ap.parse_args()
    cc = args.cc if os.path.isabs(args.cc) else os.path.join(ROOT, args.cc)
    if not os.path.exists(cc):
        print('[SKIP] 环境未就绪: %s 不存在' % cc)
        return 2
    print('# STATIC-SILENT 判据  cc=%s' % os.path.basename(cc))
    ok = True

    img = size_of_image(cc)
    mb = img / 1048576.0
    good = mb <= LIMIT_MB
    ok = ok and good
    print('[足迹] SizeOfImage = %.1f MiB  <= %d MiB : %s' % (mb, LIMIT_MB, 'PASS' if good else 'FAIL'))

    os.makedirs(WORK, exist_ok=True)
    src = os.path.join(WORK, 'labels.c')
    with open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(gen_labels(LABELS))
    exe = os.path.join(WORK, 'labels.exe')
    r = subprocess.run([cc, src, '-o', exe], cwd=ROOT, capture_output=True,
                       env=dict(os.environ, QCC_SMALL_HEAP='1'), timeout=300)
    txt = (r.stdout + r.stderr).decode('utf-8', 'replace')
    if r.returncode != 0:
        print('[功能] 编译失败 rc=%d : FAIL  %s' % (r.returncode, txt.strip().replace('\n', ' | ')[:160]))
        return 1
    e = subprocess.run([exe], capture_output=True, timeout=60)
    out = e.stdout.decode('utf-8', 'replace').strip()
    good2 = (e.returncode == 0 and out == 'LBL %d' % LABELS)
    ok = ok and good2
    print('[功能] %d 段前向分支 ⟹ 运行 %r（期望 %r；rc=%d） : %s'
          % (LABELS, out, 'LBL %d' % LABELS, e.returncode, 'PASS' if good2 else 'FAIL'))
    print('# 汇总: %s' % ('足迹与按需补丁表都合格' if ok else '有红'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
