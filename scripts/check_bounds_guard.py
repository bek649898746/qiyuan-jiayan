#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""边界护栏判据器（A6101 / A6102）。

驱动的是**编译器真实检查点**：用 `QCC_FI_NODE` / `QCC_FI_SLOT` 把失真值送进
`旁系()` / `调导入()`（这两个函数是静态函数，无法从外部直接调用；故障注入
是唯一能在不改语义的前提下驱动同一检查点的方式）。

断言：
1. 不注入：探针正常编译（rc=0），说明加入护栏后正常路径无回归。
2. `QCC_FI_NODE=1`：非 0xC0000005 退出，stderr 含 `A6101`。
3. `QCC_FI_SLOT=1` + `-c`：非 0xC0000005 退出，stderr 含 `A6102`。

用法：python scripts/check_bounds_guard.py --cc <编译器.exe>
"""
import argparse
import os
import subprocess
import sys
import tempfile
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ACCESS_VIOLATION = 0xC0000005


def run(cc, args, env_extra=None):
    env = os.environ.copy()
    if env_extra:
        env.update(env_extra)
    r = subprocess.run([cc] + args, capture_output=True, timeout=600, env=env)
    out = (r.stdout + r.stderr).decode('utf-8', 'replace')
    return r.returncode, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v1.exe'))
    args = ap.parse_args()
    cc = args.cc
    if not os.path.isabs(cc):
        cc = os.path.join(ROOT, cc)
    tmp = tempfile.mkdtemp(prefix='bounds_')

    # 普通探针：`{}` 插值会调用 旁系()（16798 行：占位符 → 参数类型码）
    plain = os.path.join(tmp, 'plain.c')
    open(plain, 'w', encoding='utf-8').write(
        '函数 整 主(空) {\n'
        '    整 a = 7;\n'
        '    格式化 "F {}\\n", a;\n'
        '    返 0;\n'
        '}\n')

    # 导入探针：带 printf，-c 模式下会走 链接模式 的 调导入()
    imp = os.path.join(tmp, 'imp.c')
    open(imp, 'w', encoding='utf-8').write(
        '#include <stdio.h>\nint main(void) { printf("B %d\\n", 5); return 0; }\n')

    fails = []

    rc, out = run(cc, [plain, '-o', os.path.join(tmp, 'plain.exe')])
    print('[%s] 正常路径: rc=%d' % ('PASS' if rc == 0 else 'FAIL', rc))
    if rc != 0:
        fails.append('正常路径 rc=%d\n%s' % (rc, out[-500:]))

    rc, out = run(cc, [plain, '-o', os.path.join(tmp, 'fi_node.exe')],
                  {'QCC_FI_NODE': '1'})
    ok = rc != ACCESS_VIOLATION and 'A6101' in out
    print('[%s] QCC_FI_NODE: rc=%d (0x%08X) 含A6101=%s'
          % ('PASS' if ok else 'FAIL', rc, rc & 0xFFFFFFFF, 'A6101' in out))
    if not ok:
        fails.append('A6101 未触发: rc=%d out=%s' % (rc, out[-400:]))

    rc, out = run(cc, ['-c', imp, '-o', os.path.join(tmp, 'imp.o')],
                  {'QCC_FI_SLOT': '1'})
    ok = rc != ACCESS_VIOLATION and 'A6102' in out
    print('[%s] QCC_FI_SLOT: rc=%d (0x%08X) 含A6102=%s'
          % ('PASS' if ok else 'FAIL', rc, rc & 0xFFFFFFFF, 'A6102' in out))
    if not ok:
        fails.append('A6102 未触发: rc=%d out=%s' % (rc, out[-400:]))

    print('[check_bounds_guard] %d 项 · 失败 %d' % (3, len(fails)))
    for f in fails:
        print('  ✗', f)
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
