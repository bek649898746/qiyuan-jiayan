# -*- coding: utf-8 -*-
"""check_assign_lvalue —— **非左值赋值被静默接受**（台账 ASSIGN-NONLVALUE）。

背景（2026-10-27 R888 收口时，skeptic 用 `取(0) *p = 5;` 反证终结符洞时连带挖出）：
  甲言 对赋值目标**不做左值校验**：`1 * 丙 = 5;` / `(取(0) + 1) = 5;` / `取(0) *丙 = 5;`
  全部 **rc=0**，且运行后目标变量**原样不变**（赋值被静默丢弃）—— 属「零静默」家族的
  **赋值目标** 一侧（与语句终结符无关：`1 * 丙 = 5;` 里根本没有尾随词元问题）。

判据（本闸 = 缺陷复现器，洞在则红）：
  · 非左值赋值必须被拒（rc≠0 且输出含 A1004/A1007/A3xxx 之一，而不是崩溃）；
  · 对照：`丙 = 5;`（指针本身）/ `*丙 = 5;` / `甲 = 5;` 必须 rc=0。

用法: python scripts/check_assign_lvalue.py [--cc <exe>]（默认取 QCC_TEST_BIN，再退回 seed）
退出: 0 = 已收口；1 = 洞仍在（静默接受）；2 = 环境/对照异常
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HEAD = ('#包含 <stdio.h>\n静 整 取(整 甲) { 返 甲; }\n'
        '静 整 甲 = 0;\n静 整 *丙 = &甲;\n')
CASES = [
    ('lit-mul', '    1 * 丙 = 5;\n'),
    ('paren-add', '    (取(0) + 1) = 5;\n'),
    ('call-mul', '    取(0) *丙 = 5;\n'),
]
CONTROLS = [
    ('ptr-var', '    丙 = &甲;\n'),
    ('deref', '    *丙 = 5;\n'),
    ('scalar', '    甲 = 5;\n'),
]
OK_MARKS = ('A1004', 'A1007', 'A3101', 'A3106', 'A9005', 'A3003')


def compile_one(cc, body, workdir, tag):
    src = os.path.join(workdir, tag + '.c')
    with open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(HEAD + '函数 整 主(空) {\n' + body + '    返 0;\n}\n')
    r = subprocess.run([cc, src, '-o', src + '.exe'], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    return r.returncode, ((r.stdout or '') + (r.stderr or '')).strip().replace('\n', ' ')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=None,
                    help='默认取 QCC_TEST_BIN，再退回 qcc_zh_seed.exe（`run_criteria --product` 走环境变量）')
    a = ap.parse_args()
    cc = a.cc or os.environ.get('QCC_TEST_BIN') or os.path.join(REPO, 'qcc_zh_seed.exe')
    cc = cc if os.path.isabs(cc) else os.path.join(REPO, cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 2

    w = tempfile.mkdtemp(prefix='qcc_lvalue_')
    holes, wrong = [], []
    for tag, body in CASES:
        rc, out = compile_one(cc, body, w, tag)
        rejected = rc != 0 and any(m in out for m in OK_MARKS)
        print('  [%-10s] rc=%d %s' % (tag, rc, '' if rejected else '← 未按预期拒绝（静默接受）'))
        if not rejected:
            holes.append(tag)
    for tag, body in CONTROLS:
        rc, out = compile_one(cc, body, w, tag)
        print('  [%-10s] rc=%d（对照：应为 0）' % (tag, rc))
        if rc != 0:
            wrong.append('%s(%s)' % (tag, out[:80]))

    if wrong:
        print('[FAIL] 合法赋值被拒: %s' % wrong)
        return 2
    if holes:
        print('[BLOCKED] ASSIGN-NONLVALUE: 非左值赋值仍被静默接受（赋值被丢弃）: %s' % holes)
        return 1
    print('[PASS] ASSIGN-NONLVALUE: 非左值赋值均被响亮拒绝，合法赋值不受影响')
    return 0


if __name__ == '__main__':
    sys.exit(main())
