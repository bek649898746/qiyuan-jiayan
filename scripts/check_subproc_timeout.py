#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_subproc_timeout —— ORD-20261104-122：子进程超时护栏的验收判据。

A. 负例：import qcc_tmp 的脚本跑**死循环产物** ⟹ `TimeoutExpired`（补丁生效）。
B. 对照组：不 import qcc_tmp 的同一调用 ⟹ 仍挂着（证明是补丁在起作用，不是环境）。
C. 正例：T1 高危判据（check_elf_backend）不误杀。
D. 静态：`check_opt_per_pass.py` 的 `cm_*` 路径带 timeout。

用法: python scripts/check_subproc_timeout.py
退出码: 0 = A/B/C/D 全成立
"""
import io
import os
import subprocess
import sys
import tempfile
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(ROOT, 'scripts')
LOOP = 'import time\nwhile True:\n    time.sleep(1)\n'


def main():
    red = []
    with tempfile.TemporaryDirectory(prefix='qcc_subto_') as tmp:
        loop = os.path.join(tmp, 'loop.py')
        io.open(loop, 'w', encoding='utf-8').write(LOOP)

        # A. with qcc_tmp
        a = os.path.join(tmp, 'a.py')
        code_a = (
            'import sys, subprocess, time\n'
            'sys.path.insert(0, ' + repr(SCRIPTS) + ')\n'
            'import qcc_tmp\n'
            't=time.time()\n'
            'try:\n'
            '    subprocess.run([sys.executable, ' + repr(loop) + '])\n'
            '    print("NO-TIMEOUT")\n'
            'except subprocess.TimeoutExpired:\n'
            '    print("TIMEOUT", round(time.time()-t, 1))\n')
        io.open(a, 'w', encoding='utf-8').write(code_a)
        env = dict(os.environ)
        env['QCC_SUBPROC_TIMEOUT'] = '2'
        r = subprocess.run([sys.executable, a], capture_output=True, text=True, timeout=30, env=env)
        okA = r.returncode == 0 and 'TIMEOUT' in (r.stdout or '')
        print('[%s] A 有补丁：%s' % ('PASS' if okA else 'FAIL', (r.stdout or r.stderr).strip()[:80]))
        if not okA:
            red.append('A')

        # B. without qcc_tmp (control) — spawn and kill after 3s
        b = os.path.join(tmp, 'b.py')
        io.open(b, 'w', encoding='utf-8').write(
            'import sys, subprocess, time\n'
            'subprocess.run([sys.executable, %r])\n' % loop)
        p = subprocess.Popen([sys.executable, b], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(3)
        alive = (p.poll() is None)
        # ⚠️ 收尸必须**连子进程树**一起 —— b.py 自己会再起一个死循环的 loop.py；
        #    只 kill b.py 会留下孤儿 loop.py 永久空转（实测：本轮全量判据里就漏了一个，
        #    pid 22640 挂了 >8 分钟）。这与「复发 #5：后台任务必须显式收尸」同一条铁律。
        subprocess.run(['taskkill', '/F', '/T', '/PID', str(p.pid)],
                       capture_output=True)
        try:
            p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            p.kill()
        okB = alive
        print('[%s] B 无补丁对照组：3s 后仍运行=%s（已连树收尸）' % ('PASS' if okB else 'FAIL', alive))
        if not okB:
            red.append('B')

    # C. positive: no false kill
    t0 = time.time()
    r = subprocess.run([sys.executable, os.path.join(SCRIPTS, 'check_elf_backend.py')],
                       cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=600)
    okC = (r.returncode == 0)
    print('[%s] C 正例 check_elf_backend rc=%d %.1fs' % ('PASS' if okC else 'FAIL', r.returncode, time.time() - t0))
    if not okC:
        red.append('C')

    # D. static: cm_* path carries timeout
    src = io.open(os.path.join(SCRIPTS, 'check_opt_per_pass.py'), 'rb').read().decode('utf-8', 'surrogateescape')
    idx = src.find("cm_%02d")
    seg = src[idx:idx + 1500] if idx >= 0 else ''
    okD = 'timeout=' in seg
    print('[%s] D check_opt_per_pass cm_* 路径带 timeout=%s' % ('PASS' if okD else 'FAIL', okD))
    if not okD:
        red.append('D')

    if red:
        print('=== 子进程超时护栏: FAIL -> %s ===' % red)
        return 1
    print('=== 子进程超时护栏: PASS（A/B/C/D）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
