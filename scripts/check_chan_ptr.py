# -*- coding: utf-8 -*-
"""check_chan_ptr — 通道缓冲区必须能完整承载 8 字节元素（ORD-20261030-60 子项）。

## 由来

原生成 `整数 __ch_q_buf[n]`（4 字节元素）⟹ `送 q, 指针;` 时高 32 位被**静默丢弃**，
收回来的是截断值，rc=0 无诊断。2026-10-31 改为 8 字节槽。

## 判据

同一通道依次存取：
  A 指针：`单 *p` 送入再收回，必须 `r == p`
  B long long：`5000000000`（> 2^32）送入再收回，必须相等
两条都用**比较**判定（不用 %lld，运行时无该格式符）。

用法: python scripts/check_chan_ptr.py [--cc qcc_zh_v1.exe]
退出码：0 = 8 字节完整；1 = 截断（缺陷存在）。
"""
import argparse
import io
import os
import subprocess
import sys
import sys as _sys_t2
_sys_t2.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402  A1

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = tmp_dir('scratch_chan_ptr')

SRC = '''# 包含 <stdio.h>
函数 整 主(空){
    单 fa = 1.5f, fb = 2.5f;
    单 *p = &fa;
    通道 q, 4;
    送 q, p;
    单 *r = 0;
    收 q, r;
    若 (r == p) { 格式化 "PTR-OK\\n", 0; } 否 { 格式化 "PTR-BAD\\n", 0; }
    long long 大 = 5000000000;
    送 q, 大;
    long long 回 = 0;
    收 q, 回;
    若 (回 == 5000000000) { 格式化 "LL-OK\\n", 0; } 否 { 格式化 "LL-BAD\\n", 0; }
    返 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(WORK, 'chan_ptr.jy')
    exe = os.path.join(WORK, 'chan_ptr.exe')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(SRC)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    if r.returncode != 0:
        print('[编译] rc=%d : %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:200]))
        return 1
    q = subprocess.run([exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=60)
    out = (q.stdout or '')
    print('[run] %s' % out.strip().replace('\n', ' / '))
    fails = []
    if 'PTR-OK' not in out:
        fails.append('通道存指针丢高 32 位')
    if 'LL-OK' not in out:
        fails.append('通道存 long long 丢高 32 位')
    if fails:
        for f in fails:
            print('[FAIL] ' + f)
        return 1
    print('[PASS] 通道 8 字节元素完整（指针 + long long）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
