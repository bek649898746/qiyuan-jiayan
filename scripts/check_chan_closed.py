# -*- coding: utf-8 -*-
"""check_chan_closed — 关闭后的通道必须**响亮拒绝**写入（ORD-20261030-60 子项③）。

## 由来

`关闭通道 q, n;` 只置 `__ch_q_closed = 1`，而 `送` / `背压 送` 完全不读它
⟹ 关闭后仍可写入，**静默接受**（rc=0，无诊断）。2026-10-31 在 `造送块` /
`造背压送块` 开头加 `若 (__ch_q_closed > 0) { printf("通道已关闭\n"); exit(1); }`。

## 判据

  A 关闭后送：必须 exit≠0 且输出含「通道已关闭」
  B 正常送：必须正常完成（不误伤）

用法: python scripts/check_chan_closed.py [--cc qcc_zh_v1.exe]
退出码：0 = 两条都对；1 = 有红。
"""
import argparse
import io
import os
import subprocess
import sys
import sys as _sys_tmp
_sys_tmp.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402  A1

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = tmp_dir('scratch_chan_closed')

CLOSED_SRC = '''# 包含 <stdio.h>
函数 整 主(空){
    通道 q, 4;
    送 q, 7;
    关闭通道 q, 4;
    送 q, 9;
    格式化 "未拦截\\n", 0;
    返 0;
}
'''

CLOSED_RECV_SRC = '''# 包含 <stdio.h>
函数 整 主(空){
    通道 q, 2;
    关闭通道 q, 2;
    整 v = 0;
    收 q, v;
    格式化 "NO-ABORT\\n", 0;
    返 0;
}
'''

DRAIN_SRC = '''# 包含 <stdio.h>
函数 整 主(空){
    通道 q, 4;
    送 q, 7;
    关闭通道 q, 4;
    整 v = 0;
    收 q, v;
    格式化 "DRAIN v=%d\\n", v;
    返 0;
}
'''

OK_SRC = '''# 包含 <stdio.h>
函数 整 主(空){
    通道 q, 4;
    送 q, 7;
    整 v = 0; 收 q, v;
    格式化 "正常 %d\\n", v;
    返 0;
}
'''


def build(cc, name, src):
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(WORK, name + '.jy')
    exe = os.path.join(WORK, name + '.exe')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    return r.returncode, ((r.stdout or '') + (r.stderr or '')), exe


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    fails = []

    rc, err, exe = build(cc, 'closed', CLOSED_SRC)
    if rc != 0:
        fails.append('关闭用例编译失败: %s' % err[:160])
    else:
        q = subprocess.run([exe], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=30)
        out = (q.stdout or '')
        print('[A 关闭后送] rc=%d out=%r err=%r' % (q.returncode, out.strip(), (q.stderr or '').strip()))
        if q.returncode == 0 or '通道已关闭' not in (out + (q.stderr or '')):
            fails.append('关闭后仍可写入（rc=%d, out=%r）' % (q.returncode, out.strip()))

    rc, err, exe = build(cc, 'ok', OK_SRC)
    if rc != 0:
        fails.append('正常用例编译失败: %s' % err[:160])
    else:
        q = subprocess.run([exe], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=30)
        out = (q.stdout or '')
        print('[B 正常送] rc=%d out=%r' % (q.returncode, out.strip()))
        if q.returncode != 0 or '正常 7' not in out:
            fails.append('正常送 被误伤（rc=%d, out=%r）' % (q.returncode, out.strip()))

    # 腿 C：关闭后 `收` 必须中止（施工单 §4.1「关闭空收返回失败」）
    rc, err, exe = build(cc, 'closed_recv', CLOSED_RECV_SRC)
    if rc != 0:
        fails.append('关闭收用例编译失败: %s' % err[:160])
    else:
        q = subprocess.run([exe], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=30)
        out = (q.stdout or '')
        print('[C 关闭后收] rc=%d out=%r err=%r' % (q.returncode, out.strip(), (q.stderr or '').strip()))
        if q.returncode == 0 or '通道已关闭' not in (out + (q.stderr or '')):
            fails.append('关闭后仍可收（rc=%d, out=%r）' % (q.returncode, out.strip()))

    # 腿 D：关闭后**有存量**必须可 drain（施工单 §4.1「取空后才返回失败」）
    rc, err, exe = build(cc, 'drain', DRAIN_SRC)
    if rc != 0:
        fails.append('drain 用例编译失败: %s' % err[:160])
    else:
        q = subprocess.run([exe], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=30)
        out = (q.stdout or '')
        print('[D 关闭后有数据收] rc=%d out=%r' % (q.returncode, out.strip()))
        if q.returncode != 0 or 'DRAIN v=7' not in out:
            fails.append('关闭后存量不可 drain（rc=%d, out=%r）⟹ 数据丢失' % (q.returncode, out.strip()))

    if fails:
        for f in fails:
            print('[FAIL] ' + f)
        return 1
    print('[PASS] 关闭后送 响亮中止，正常送 不受影响')
    return 0


if __name__ == '__main__':
    sys.exit(main())
