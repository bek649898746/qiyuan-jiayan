# -*- coding: utf-8 -*-
"""check_opt_governance.py —— R16/R17 pass 治理判据（ORD-20261110-GOVERN）.

四项：
  A. 顺序约束自检：`--swap-pass 2 3`（结活算 ↔ 环提）⟹ **非零退出**，消息同时点名两者；
  B. 必需分级：`--no-cfg` / `--no-live` ⟹ **拒绝启动**（非零退出 + 消息点名被拒 pass）；
  C. 正常配置 rc=0；
  D. 治理前后**产物逐字节相同**：用 `--old-rev` 取治理前的 opt_zh.jy，编译旧优化器，
     与新版 `--no-4b`（关掉本轮新增的死区 pass）对同一 .asm 的产物比 sha256。

用法: python scripts/check_opt_governance.py [--cc qcc_zh_v2.exe] [--old-rev 3525f694]
退出码: 0 = PASS / 1 = FAIL / 2 = 环境未就绪
"""
import argparse
import hashlib
import io
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

PROBE = '#include <stdio.h>\nint main(void){ int g = 1; if (0) { g = g * 3 + 1; g = g * 3 + 2; } printf("%d\\n", g); return 0; }\n'


def run(cmd, timeout=1800):
    r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=timeout)
    return r.returncode, (r.stdout or '') + (r.stderr or '')


def sha(p):
    return hashlib.sha256(io.open(p, 'rb').read()).hexdigest()[:16]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('--opt', default='opt_zh.exe')
    ap.add_argument('--old-rev', default='3525f694', help='治理前的提交（含旧 opt_zh.jy）')
    a = ap.parse_args()
    for p in (a.cc, a.opt, 'asm_zh.exe'):
        if not os.path.exists(p):
            print('环境未就绪：缺 %s' % p)
            return 2

    work = tmp_dir('opt_gov')
    bad = []
    src = os.path.join(work, 'g.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(PROBE)
    rc, out = run([a.cc, src, '-S', '-o', os.path.join(work, 'g.exe')])
    asm = os.path.join(work, 'g.exe.asm')
    if rc != 0 or not os.path.exists(asm):
        print('探针 -S 失败 rc=%d' % rc)
        return 1

    # A 顺序违规
    rc, out = run([a.opt, asm, '-o', os.path.join(work, 'a.asm'), '--swap-pass', '2', '3'])
    print('A --swap-pass 2 3: rc=%d' % rc)
    print('   ' + ' / '.join(l for l in out.splitlines() if '违规' in l or 'ERROR' in l)[:300])
    if rc == 0 or '顺序 违规' not in out or '结活算' not in out or '环提' not in out:
        bad.append('A 顺序违规未按预期报红/点名')

    # B 必需 pass 禁关
    for flag, name in (('--no-cfg', '结建'), ('--no-live', '结活算')):
        rc, out = run([a.opt, asm, '-o', os.path.join(work, 'b.asm'), flag])
        line = ' / '.join(l for l in out.splitlines() if 'ERROR' in l)[:220]
        print('B %s: rc=%d  %s' % (flag, rc, line))
        if rc == 0 or '不可关闭' not in out or name not in out:
            bad.append('B %s 未拒绝启动' % flag)

    # C 正常配置
    rc, out = run([a.opt, asm, '-o', os.path.join(work, 'c.asm')])
    print('C 正常配置: rc=%d' % rc)
    if rc != 0:
        bad.append('C 正常配置 rc=%d' % rc)

    # D 治理前后逐字节相同
    raw = subprocess.run(['git', 'show', '%s:srclib/opt_zh.jy' % a.old_rev], capture_output=True).stdout
    if not raw:
        bad.append('D 取不到旧 opt_zh（rev=%s）' % a.old_rev)
    else:
        old_src = os.path.join(work, 'opt_old.jy')
        old_exe = os.path.join(work, 'opt_old.exe')
        io.open(old_src, 'wb').write(raw)
        rc, out = run([a.cc, old_src, '-o', old_exe])
        if rc != 0:
            bad.append('D 旧优化器编译失败 rc=%d' % rc)
        else:
            o1 = os.path.join(work, 'old_out.asm')
            o2 = os.path.join(work, 'new_out.asm')
            rc1, _ = run([old_exe, asm, '-o', o1])
            rc2, _ = run([a.opt, asm, '-o', o2, '--no-4b'])
            s1, s2 = sha(o1), sha(o2)
            print('D 治理前=%s 治理后(--no-4b)=%s %s' % (s1, s2, 'EQUAL' if s1 == s2 else 'DIFF'))
            if rc1 != 0 or rc2 != 0 or s1 != s2:
                bad.append('D 正常配置产物与治理前不同 (%s vs %s)' % (s1, s2))

    print()
    if bad:
        print('[失败 FAIL] check_opt_governance：')
        for b in bad:
            print('  - ' + b)
        return 1
    print('[通过 PASS] check_opt_governance：对调即红（点名违规对）· 必需禁关即拒 · 正常配置逐字节不变')
    return 0


if __name__ == '__main__':
    sys.exit(main())
