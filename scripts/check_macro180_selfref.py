#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_macro180_selfref —— 净缺口 #180「宏递归」的行为判据（2026-10-17）。

C 6.10.3.4 p2（blue paint）：自引用宏在展开时**不再进一步替换** ⟹ 不会无限递归。
甲言 L804 显式防重入（`#定义 A(x) A(x)` 防死递归，见主源注释）⟹ 预期行为是：
  ① **不挂死**（30s 内返回）；
  ② **响亮报错**（未声明函数 SELF ⟹ A1001），而不是静默错值；
  ③ 反向对照：把宏体改成正常表达式 `(x + 1)` 后 rc=0（证明失败只由自引用导致）。

用法:
  python scripts/check_macro180_selfref.py [--cc <exe>]
"""
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
CC = 'qcc_zh_v2.exe'
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
ANCHOR = os.path.join('scripts', 'feat_zh_macro180_selfref_neg.c')
PRE = os.path.join(REPO, 'compat_prelude.h')
TMP = os.path.join(os.environ.get('TEMP', '.'), '_check_macro180')


def main():
    os.makedirs(TMP, exist_ok=True)
    # ⓪ 正锚：合法宏链 / 宏内宏展开正常（闸内正锚 feat_zh_macro180_chain_ok.c）
    pos_src = os.path.join('scripts', 'feat_zh_macro180_chain_ok.c')
    pos_exe = os.path.join(TMP, 'chain.exe')
    rp = subprocess.run([CC, '-I', PRE, pos_src, '-o', pos_exe], capture_output=True, timeout=180)
    out = ''
    rc_run = -1
    if rp.returncode == 0:
        rr = subprocess.run([pos_exe], capture_output=True, timeout=60)
        out = (rr.stdout + rr.stderr).decode('utf-8', 'replace').strip()
        rc_run = rr.returncode
    ok0 = (rp.returncode == 0 and rc_run == 0 and 'MAC180 42 42' in out)
    print('positive: compile rc=%d run rc=%d out=%r -> %s' % (
        rp.returncode, rc_run, out[:80], 'PASS' if ok0 else 'FAIL'))

    # ① 自引用：不超时 + rc!=0 + A1001
    try:
        r = subprocess.run([CC, '-I', PRE, '-c', ANCHOR, '-o', os.path.join(TMP, 'a.o')],
                           capture_output=True, timeout=30)
        msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
        timed_out = False
    except subprocess.TimeoutExpired:
        r, msg, timed_out = None, 'TIMEOUT(30s)', True
    print('self-ref: timeout=%s rc=%s has_A1001=%s' % (
        timed_out, '-' if r is None else r.returncode, 'A1001' in msg))
    ok1 = (not timed_out) and r is not None and r.returncode != 0 and 'A1001' in msg

    # ② 反向对照：正常宏应 rc=0
    src = open(ANCHOR, encoding='utf-8').read()
    plain = src.replace('// @EXPECTED compile_fail\n', '').replace('#define SELF(x) SELF(x)', '#define SELF(x) (x + 1)')
    p = os.path.join(TMP, 'plain.c')
    open(p, 'w', encoding='utf-8').write(plain)
    r2 = subprocess.run([CC, '-I', PRE, '-c', p, '-o', os.path.join(TMP, 'p.o')],
                        capture_output=True, timeout=60)
    m2 = ((r2.stdout or b'') + (r2.stderr or b'')).decode('utf-8', 'replace')
    ok2 = (r2.returncode == 0)
    print('control : rc=%d %s' % (r2.returncode, 'OK' if ok2 else m2.strip().replace('\n', ' | ')[:120]))

    ok = ok0 and ok1 and ok2
    print('=== check_macro180_selfref: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
