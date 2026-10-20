# -*- coding: utf-8 -*-
"""check_refusal_rc — D32b 判据（2026-10-21 虾米）：**拒绝执行必须非零退出**

病灶：`run_tests_zh.py` 的内存预检拒绝分支写的是 `return 2`，但入口是 `main()`（不是
`sys.exit(main())`）⟹ 拒绝执行时脚本 **退出码 0** ⟹ 上层脚本/CI 把「没跑」当「全绿」
（与 D32「零锚假绿」同族）。

判据：
 ① 用不可能满足的并发（`--parallel 99`）跑一个锚 ⟹ **rc == 2** 且 stderr 含「拒绝执行」
 ② 正常并发（`--parallel 1`）跑同一锚 ⟹ **rc == 0** 且含 `PASS=1`（证明只在拒绝时非零）

用法: python scripts/check_refusal_rc.py
"""
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
ANCHOR = 'scripts/feat_zh_typeof_ptr_ok.c'


def run(par):
    return subprocess.run([sys.executable, 'scripts/run_tests_zh.py', '--only', ANCHOR,
                           '--parallel', str(par)], capture_output=True, text=True,
                          encoding='utf-8', errors='replace', timeout=600)


def main():
    r = run(99)
    out = (r.stdout or '') + (r.stderr or '')
    ok1 = (r.returncode == 2 and '拒绝执行' in out)
    print('① 拒绝执行: rc=%d 含拒绝=%s -> %s' % (r.returncode, '拒绝执行' in out, 'PASS' if ok1 else 'FAIL'))
    r2 = run(1)
    out2 = (r2.stdout or '') + (r2.stderr or '')
    ok2 = (r2.returncode == 0 and 'PASS=1' in out2.replace(' ', ''))
    print('② 正常执行: rc=%d 含PASS=1=%s -> %s' % (r2.returncode, 'PASS=1' in out2.replace(' ', ''),
                                                'PASS' if ok2 else 'FAIL'))
    ok = ok1 and ok2
    print('=== D32b 判据（拒绝 ⟹ 非零）: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
