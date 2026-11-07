#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_only —— S0 判据（ORD-20261103-102 / ORD-20261104-108 的第一刀）。

S0 = 「IR 从码生成的影子变成可独立产出的一段」：`--ir-only` 只建 IR + dump，**不写产物**。

四条一起验，缺一不算：
  A 非空：`--ir-only` 出一份**空不了**的 IR（行数 > 1000，且打印 `[IR-ONLY]` 与 L0/档 计数）；
  B 等价：与 `QCC_DUMP=ir` 的 dump **逐行相同**（去掉解析期侧表前缀 —— 那是 `转储()` 的诊断输出，
    不是 IR 本体；归一化锚点 = 首个 `-- IR L1 抽象指令` 行）；
  C 负例：`QCC_IR_NOREC=1`（关掉 IR 记录）⟹ 必须 `[ERR] IR-EMPTY` + 非零退出，不许静默出空 IR；
  D 零产物影响：同源同探针，新编译器与基线编译器产物 **SHA256 相同**（本刀不许动产物）。

判据自己从**当前源码**编一个编译器（`srclib_jiayan/qcc_zh.jy` → 临时 exe）再测，保证跑的是 shipped code。

用法: python scripts/check_ir_only.py [--probe scripts/feat_zh_ifacem_ok.c]
退出码: 0 = 四条全过；1 = 任一不过。
"""
import argparse
import hashlib
import io
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MARK = '-- IR L1 抽象指令'
MIN_LINES = 1000


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def norm(txt):
    lines = txt.splitlines()
    for i, l in enumerate(lines):
        if l.startswith(MARK):
            return lines[i:]
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--probe', default='scripts/feat_zh_ifacem_ok.c')
    a = ap.parse_args()
    probe = os.path.join(ROOT, a.probe)
    if not os.path.exists(probe):
        print('[FAIL] 探针不存在: %s' % a.probe)
        return 1

    work = tmp_dir('ironly')
    base_env = dict(os.environ)
    base_env['QCC_RT_PATH'] = os.path.join(ROOT, 'srclib', 'qcc_rt.c')
    bad = []
    try:
        cc = os.path.join(work, 'cc.exe')
        b = subprocess.run([os.path.join(ROOT, 'qcc_zh_seed.exe'),
                            os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy'), '-o', cc],
                           cwd=ROOT, capture_output=True)
        if b.returncode != 0 or not os.path.exists(cc):
            print('[FAIL] 从当前源码编编译器失败 rc=%d' % b.returncode)
            return 1

        r1 = subprocess.run([cc, probe, '--ir-only'], cwd=work, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', env=base_env)
        lines1 = r1.stderr.splitlines()
        print('[A] --ir-only rc=%d · IR 行数=%d · stdout=%r'
              % (r1.returncode, len(lines1), (r1.stdout or '').strip()[-60:]))
        if r1.returncode != 0 or len(lines1) < MIN_LINES or '[IR-ONLY]' not in (r1.stdout or ''):
            bad.append('A 非空/成功')

        env = dict(os.environ); env['QCC_DUMP'] = 'ir'
        env['QCC_RT_PATH'] = base_env['QCC_RT_PATH']
        out = os.path.join(work, 'p.exe')
        r2 = subprocess.run([cc, probe, '-o', out], cwd=work, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', env=env)
        n1, n2 = norm(r1.stderr), norm(r2.stderr)
        print('[B] 归一化: ir-only %d 行 · dump %d 行 ⟹ %s'
              % (len(n1), len(n2), '逐行相同' if n1 == n2 else '不同'))
        if r2.returncode != 0 or n1 != n2 or len(n1) < MIN_LINES:
            bad.append('B 逐行等价')

        env2 = dict(os.environ); env2['QCC_IR_NOREC'] = '1'
        env2['QCC_RT_PATH'] = base_env['QCC_RT_PATH']
        r3 = subprocess.run([cc, probe, '--ir-only'], cwd=work, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', env=env2)
        print('[C] 负例 rc=%d · marker=%s' % (r3.returncode, 'IR-EMPTY' in r3.stderr))
        if r3.returncode == 0 or 'IR-EMPTY' not in r3.stderr:
            bad.append('C 负例未响亮失败')

        base = os.path.join(ROOT, 'qcc_zh_v1.exe')
        x = os.path.join(work, 'x.exe'); y = os.path.join(work, 'y.exe')
        subprocess.run([base, probe, '-o', x], cwd=work, capture_output=True, env=base_env)
        subprocess.run([cc, probe, '-o', y], cwd=work, capture_output=True, env=base_env)
        hx = sha256(x) if os.path.exists(x) else '-'
        hy = sha256(y) if os.path.exists(y) else '-'
        print('[D] 产物 SHA: 基线 %s… · 新 %s… ⟹ %s' % (hx[:12], hy[:12], '相同' if hx == hy else '不同'))
        if hx != hy:
            bad.append('D 产物变了')
    finally:
        shutil.rmtree(work, ignore_errors=True)

    if bad:
        print('=== S0 判据: FAIL -> %s ===' % bad)
        return 1
    print('=== S0 判据: PASS（ir-only 非空 · 与 dump 逐行等价 · 负例响亮 · 零产物影响）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
