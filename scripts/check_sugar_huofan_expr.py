# -*- coding: utf-8 -*-
"""check_sugar_huofan_expr —— 「表达式语句 + 尾随词元」缺陷复现器（台账 SUGAR-HUOFAN-EXPR / R888）。

背景：`语句()` 的泛表达式末梢原先**无条件 `词位++`**，把表达式之后的**任何**非终结符词元当 `;` 吃掉：
      标识符 / 常数 / 字符串 / 运算符 / 关键字 / 开块 —— 全部 rc=0（静默），而单独写它们是 A1001 等响亮诊断。
      R887 只拦了「词变量」一种，被 skeptic 正确打回「洞没除根」；R888 改为**终结符收口**：
      `;` 消费、`}`/词终 放行、其余一律 A1004。

判据（本闸 = 缺陷复现器，洞在则红）：
  · 每种尾随词元形态都必须 **rc≠0 且 输出含 A1004**（不接受「编译器崩溃也算通过」）；
  · 对照组（合法写法：正常调用 / 延迟 / 块尾）必须 rc=0，防止我们为了变绿把好代码也拒了。

用法: python scripts/check_sugar_huofan_expr.py [--cc <exe>]   （默认取 QCC_TEST_BIN，再退回 seed）
退出: 0 = 洞已消失（全部形态 A1004，对照全绿）；1 = 仍有静默通过；2 = 环境/对照异常
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HEAD = '#包含 <stdio.h>\n静 整 取(整 甲) { 返 甲; }\n静 空 记(空) { }\n静 整 甲 = 0;\n静 整 *丙 = &甲;\n'
CASES = [
    ('ident', '    取(0) 甲乙;\n'),
    ('huofan', '    取(0) 或返;\n'),
    ('number', '    取(0) 42;\n'),
    ('float', '    取(0) 1.5;\n'),
    ('string', '    取(0) "x";\n'),
    ('keyword', '    取(0) 返;\n'),
    ('block', '    取(0) { 取(1); }\n'),
    ('char', "    取(0) 'x';\n"),
    ('ident-assign', '    取(0) 甲 = 5;\n'),
]
CONTROLS = [
    ('normal-call', '    取(0);\n'),
    ('defer', '    记();\n'),
    ('block-tail', '    若 (1) { 取(0); }\n'),
]


def compile_one(cc, body, workdir, tag):
    src = os.path.join(workdir, tag + '.c')
    with open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(HEAD + '函数 整 主(空) {\n' + body + '    返 0;\n}\n')
    r = subprocess.run([cc, src, '-o', src + '.exe'], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    out = ((r.stdout or '') + (r.stderr or '')).strip().replace('\n', ' ')
    return r.returncode, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=None,
                    help='编译器（默认取 QCC_TEST_BIN，再退回 qcc_zh_seed.exe）——'
                         '`run_criteria --product …` 走环境变量指定修前产物，故这里必须认它')
    a = ap.parse_args()
    cc = a.cc or os.environ.get('QCC_TEST_BIN') or os.path.join(REPO, 'qcc_zh_seed.exe')
    cc = cc if os.path.isabs(cc) else os.path.join(REPO, cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 2

    w = tempfile.mkdtemp(prefix='qcc_huofan_')
    holes, wrong = [], []
    for tag, body in CASES:
        rc, out = compile_one(cc, body, w, tag)
        ok = (rc != 0 and 'A1004' in out)
        print('  [%-10s] rc=%d %s %s' % (tag, rc, 'A1004' if 'A1004' in out else '(无A1004)',
                                        '' if ok else '← 未按预期拒绝'))
        if not ok:
            holes.append(tag)
    for tag, body in CONTROLS:
        rc, out = compile_one(cc, body, w, tag)
        print('  [%-10s] rc=%d（对照：应为 0）' % (tag, rc))
        if rc != 0:
            wrong.append('%s(%s)' % (tag, out[:80]))

    if wrong:
        print('[FAIL] 合法写法被拒（不许为了变绿把好代码也拦了）: %s' % wrong)
        return 2
    if holes:
        print('[BLOCKED] SUGAR-HUOFAN-EXPR: 以下尾随词元仍被静默接受: %s'
              ' —— 语句终结符守卫未收口' % holes)
        return 1
    print('[PASS] SUGAR-HUOFAN-EXPR: 标识符/常数/浮点/字符串/关键字/块/运算符 九种尾随词元全部 A1004；对照三种合法写法不受影响')
    return 0


if __name__ == '__main__':
    sys.exit(main())
