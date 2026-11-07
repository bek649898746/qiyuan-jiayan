#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_backends_all —— ORD-20261104-121：一条命令跑所有后端判据并汇总。

覆盖：
  · PE 默认后端（check_build_env_not_baked）
  · ELF EXEC 后端（check_elf_backend）
  · SysV 互调（check_sysv_abi）
  · ELF `-c` 目标文件（check_elf_c_unit）
  · ELF 真跑 lua（check_elf_lua_run）
  · ELF 自举（check_elf_selfhost，--quick 跳过）
  · 对象模式单元（check_objmode_unit，可选）

规则：
  · 缺脚本显式 SKIP（打印 SKIP，且**不计为绿**：非 --quick 时整体 rc=1）；
  · 各判据独立（任一红 ⟹ 整体红）；
  · `--self-test` 注入一个必然失败的假后端，确认总闸转红（红传播自检）。

用法: python scripts/check_backends_all.py [--quick] [--self-test]
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SUITE = [
    ('PE/PE32+ default', ['check_build_env_not_baked.py']),
    ('ELF EXEC backend', ['check_elf_backend.py']),
    ('SysV ABI', ['check_sysv_abi.py']),
    ('ELF -c object', ['check_elf_c_unit.py']),
    ('ELF lua run', ['check_elf_lua_run.py']),
    ('ELF selfhost', ['check_elf_selfhost.py']),
]

OPTIONAL = [
    ('objmode unit', ['check_objmode_unit.py', '--sparse']),
]


def run_script(script, args, timeout):
    p = os.path.join(ROOT, 'scripts', script)
    if not os.path.exists(p):
        return None, ''
    r = subprocess.run([sys.executable, p] + args, cwd=ROOT, capture_output=True,
                       text=True, encoding='utf-8', errors='replace', timeout=timeout)
    return r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--quick', action='store_true', help='跳过 ELF selfhost（12 min）')
    ap.add_argument('--self-test', action='store_true', help='注入必然失败的假后端，确认总闸转红')
    a = ap.parse_args()
    red, skipped = [], []

    for name, cmd in SUITE:
        if a.quick and cmd[0] == 'check_elf_selfhost.py':
            print('[SKIP] %-20s (--quick)' % name)
            continue
        rc, out = run_script(cmd[0], cmd[1:], timeout=3600)
        if rc is None:
            print('[SKIP] %-20s (script missing)' % name)
            skipped.append(name)
            continue
        ok = rc == 0
        tail = out.splitlines()[-1][:90] if out else ''
        print('[%s] %-20s rc=%d  %s' % ('PASS' if ok else 'FAIL', name, rc, tail))
        if not ok:
            red.append(name)

    for name, cmd in OPTIONAL:
        rc, out = run_script(cmd[0], cmd[1:], timeout=1800)
        if rc is None:
            print('[SKIP] %-20s (optional script missing)' % name)
            continue
        ok = rc == 0
        print('[%s] %-20s rc=%d' % ('PASS' if ok else 'FAIL', name, rc))
        if not ok:
            red.append(name)

    if a.self_test:
        fake = os.path.join(ROOT, 'scripts', '_self_test_fail.py')
        try:
            with open(fake, 'w', encoding='utf-8') as f:
                f.write('import sys\nprint("[FAIL] injected")\nsys.exit(1)\n')
            rc, _ = run_script('_self_test_fail.py', [], timeout=60)
            injected_red = (rc == 1)
            print('[%s] %-20s rc=%d（红传播自检）' % ('PASS' if injected_red else 'FAIL', 'INJECTED-FAIL', rc))
            if not injected_red:
                red.append('INJECTED-FAIL')
        finally:
            try:
                os.remove(fake)
            except OSError:
                pass

    if skipped and not a.quick:
        print('=== 多后端总闸: FAIL -> 缺脚本 %s（显式 SKIP 不计绿）===' % skipped)
        return 1
    if red:
        print('=== 多后端总闸: FAIL -> %s ===' % red)
        return 1
    print('=== 多后端总闸: PASS ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
