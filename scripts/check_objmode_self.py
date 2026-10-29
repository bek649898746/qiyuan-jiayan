#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_objmode_self —— **对象模式独立判据器**（d-obj 专用；不入 CORE_FEAT，避免假绿）

判据（全部驱动已发布的编译器，无硬编码结果）：
  ① 语义保持：`scripts/objmode_probe_statics.c` 的 `-c` 产物 SHA256 必须等于
     `scripts/objmode_probe_statics.sha256`（该值由 **流式写之前** 的编译器 0A421684 捕获；
     流式重写后仍相同 ⟹ 写路径语义保持）。`--update-golden` 才允许改写。
  ② 34k 自身源 `-c` 必须 rc=0，且 .o **≤ 100MB**（round12 定下的 GREEN 标准；
     当前 595MB ⟹ RED，说明 `.data` 仍整段物化，稀疏/低高双区 未做）。
  ③ jyld 链接该 .o 必须 rc=0。
  ④ 链接产物编译 `objmode_probe_statics.c` 并运行 rc=0（堆计数器/绝对量 全部正确）。

用法: python scripts/check_objmode_self.py [--cc qcc_zh_v2.exe] [--update-golden]
退出码: 0 = 全过；1 = 有 RED
"""
import argparse
import atexit
import hashlib
import shutil
import os
import re
import subprocess
import sys
import tempfile
import time
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

MAX_OBJ = 100 * 1024 * 1024  # 100MB


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest().upper()


def run(cmd, timeout=600):
    t0 = time.time()
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                           encoding='utf-8', errors='replace')
        return p.returncode, (p.stdout or '') + (p.stderr or ''), time.time() - t0
    except subprocess.TimeoutExpired:
        return -9, 'TIMEOUT', time.time() - t0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')  # 2026-10-21: v2 无哈希校验、已静默落后 6 个提交
    ap.add_argument('--update-golden', action='store_true')
    a = ap.parse_args()

    tmp = tempfile.mkdtemp(prefix='objmode_')
    # 2026-10-20: 每次跑都在 %TEMP% 留一个约 550MB 的目录（self.exe 534MB + self.o 21MB）且从不清理 ——
    # 实测 20 次残留 14.8GB，把 C 盘灌到只剩 10GB。这里登记退出清理（正常退出/测试失败都会删）。
    atexit.register(lambda: shutil.rmtree(tmp, ignore_errors=True))
    fails = []

    # ① 语义保持（小对象逐字节）
    probe = os.path.join('scripts', 'objmode_probe_statics.c')
    out1 = os.path.join(tmp, 'probe1.o')
    rc, log, _ = run([a.cc, '-c', probe, '-o', out1])
    if rc != 0:
        fails.append('① probe -c rc=%d: %s' % (rc, log.strip()[:120]))
        got = ''
    else:
        got = sha(out1)
        golden = open(os.path.join('scripts', 'objmode_probe_statics.sha256'),
                      encoding='utf-8').read().strip().upper()
        if a.update_golden:
            open(os.path.join('scripts', 'objmode_probe_statics.sha256'), 'w',
                 encoding='utf-8', newline='\n').write(got + '\n')
            print('[①] golden 已更新 = %s' % got)
        elif got != golden:
            fails.append('① 小对象 字节 不同: got=%s want=%s' % (got, golden))
    print('[①] 小对象 sha256 = %s' % (got or '(编译失败)'))

    # ② 34k 自身源 -c
    big = os.path.join(tmp, 'self.o')
    rc, log, dt = run([a.cc, '-c', os.path.join('srclib_jiayan', 'qcc_zh.jy'), '-o', big], timeout=900)
    size = os.path.getsize(big) if os.path.exists(big) else 0
    print('[②] 34k -c: rc=%d elapsed=%.1fs size=%d (%.1fMB; 上限 100MB)' % (rc, dt, size, size / 1048576.0))
    if rc != 0:
        fails.append('② 34k -c rc=%d' % rc)
    if size > MAX_OBJ:
        fails.append('② 34k .o = %.1fMB > 100MB (仍整段物化 .data)' % (size / 1048576.0))

    # ③ 链接
    exe = os.path.join(tmp, 'self.exe')
    rc, log, _ = run([os.path.join('srclib', 'jyld.exe'), '-o', exe, big], timeout=900)
    print('[③] jyld: rc=%d %s' % (rc, log.strip().split('\n')[-1][:100]))
    if rc != 0:
        fails.append('③ jyld rc=%d' % rc)

    # ④ 链接产物 编 + 跑
    tiny_out = os.path.join(tmp, 'p.exe')
    rc, log, _ = run([exe, probe, '-o', tiny_out], timeout=120) if os.path.exists(exe) else (-1, 'no exe', 0)
    print('[④] relinked 编 probe: rc=%d' % rc)
    if rc != 0:
        fails.append('④ relinked 编译 rc=%d' % rc)
    else:
        rc2, log2, _ = run([tiny_out], timeout=60)
        print('[④] 运行: rc=%d %s' % (rc2, log2.strip()[:80]))
        if rc2 != 0:
            fails.append('④ 运行 rc=%d' % rc2)

    print()
    if fails:
        print('=== objmode self: RED (%d 项) ===' % len(fails))
        for f in fails:
            print('  - ' + f)
        return 1
    print('=== objmode self: PASS ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
