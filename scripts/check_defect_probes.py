#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_defect_probes —— 三条「语料真缺陷」的最小探针回归（2026-10-17 挂账）。

对应 `_DEBT_INDEX.md` 2026-10-17 段「git 20 条定性」里的 3 条真缺陷（第 4 条 parse-options 有独立判据器
`scripts/check_a9006_cast_const.py`）：
  kwset_constptrptr_a9006.c : `struct trie * const *next` 的 `next` 本身被当 const（A9006）—— **已最小复现**
  fsmonitor_seqnr_a9006.c   : 真实件 A9006；**最小探针未复现**（rc=0，简化后形态通过）—— 留待下一刀
  am_forward_goto_a9001.c   : 真实件 A9001；**最小探针未复现**（rc=0，单函数前向 goto 通过）—— 留待下一刀

用法:
  python scripts/check_defect_probes.py [--cc <exe>] [--expect-bug]
  --expect-bug  强制 kwset 复现 A9006；fsmonitor/am 只打印实际 rc（未复现 = 如实记录，不算 FAIL）。
  默认（修好后）  全部应 rc=0。
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
EXPECT_BUG = '--expect-bug' in sys.argv
PRE = os.path.join(REPO, 'compat_prelude.h')
TMP = os.path.join(os.environ.get('TEMP', '.'), '_check_defect_probes')

# name -> (错误码 or None, 是否在挂账模式下强制复现)
CASES = {
    'kwset_constptrptr_a9006.c': ('A9006', True),
    'fsmonitor_seqnr_a9006.c': ('A9006', False),
    'am_forward_goto_a9001.c': ('A9001', False),
}


def main():
    os.makedirs(TMP, exist_ok=True)
    hard_fails = []
    for name, (code, enforce) in CASES.items():
        src = os.path.join('scripts', 'defect_probes', name)
        out = os.path.join(TMP, name[:-2] + '.o')
        r = subprocess.run([CC, '-I', PRE, '-c', src, '-o', out], capture_output=True, timeout=180)
        msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()
        hit = re.search(r'(A\d{4})', msg)
        got = hit.group(1) if hit else '-'
        if EXPECT_BUG:
            if enforce:
                ok = (r.returncode == 1 and got == code)
            else:
                ok = True  # 如实记录
                if r.returncode == 0:
                    note = '（未复现：简化形态通过，真实件仍红）'
                else:
                    note = ''
                print('%-32s rc=%d code=%s %s' % (name, r.returncode, got, note))
                continue
        else:
            ok = (r.returncode == 0)
        print('%-32s rc=%d code=%s %s  %s' % (name, r.returncode, got,
                                              'OK' if ok else 'MISMATCH', msg.replace('\n', ' | ')[:110]))
        if not ok:
            hard_fails.append(name)
    mode = '挂账状态确认' if EXPECT_BUG else '目标状态判定'
    print('=== %s: %s ===' % (mode, 'PASS' if not hard_fails else 'FAIL: ' + ','.join(hard_fails)))
    return 0 if not hard_fails else 1


if __name__ == '__main__':
    sys.exit(main())
