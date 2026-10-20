# -*- coding: utf-8 -*-
"""check_generic_warn — S1 第一步 判据（W9601 · 2026-10-21 虾米）

背景（审计施工单 S1）：旧 `泛型 名(参数) = 体` 是**文本宏**（实参可能重复求值），
与新 `泛型函数/实例` 同名不同义。主源与全部 `.jy` **0 处**使用 ⟹ 零依赖孤儿，
只 14 个测试锚依赖。第一步 = 注册时发**非致命** W9601，不破坏既有锚。

判据（三件，全绿）：
 ① 旧语法锚 `feat_zh_generic96_ok.c` ⟹ **rc=0**（行为不变）且 stderr 含 **W9601**
 ② 真单态化锚 `feat_zh_generic_mono_ok.c` ⟹ **rc=0** 且 stderr **不含** W9601（不误报新语法）
 ③ 反向对照：把旧语法行注释掉 ⟹ 无 W9601（证明警告只由旧语法触发）

用法: python scripts/check_generic_warn.py [--cc <exe>]
退出: 0 PASS / 1 FAIL
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
OLD = os.path.join('scripts', 'feat_zh_generic96_ok.c')
NEW = os.path.join('scripts', 'feat_zh_generic_mono_ok.c')


def run(cc, src, exe):
    return subprocess.run([cc, src, '-o', exe], capture_output=True, text=True,
                          encoding='utf-8', errors='replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    a = ap.parse_args()
    d = tempfile.mkdtemp(prefix='gwarn_')
    ok = True

    r = run(a.cc, OLD, os.path.join(d, 'old.exe'))
    out = (r.stdout or '') + (r.stderr or '')
    good = (r.returncode == 0 and 'W9601' in out)
    print('① 旧语法锚 rc=%d W9601=%s -> %s' % (r.returncode, 'W9601' in out, 'PASS' if good else 'FAIL'))
    if not good:
        print('   out: %s' % out.strip()[:300])
    ok &= good

    r = run(a.cc, NEW, os.path.join(d, 'new.exe'))
    out2 = (r.stdout or '') + (r.stderr or '')
    good = (r.returncode == 0 and 'W9601' not in out2)
    print('② 真单态化锚 rc=%d 无W9601=%s -> %s' % (r.returncode, 'W9601' not in out2, 'PASS' if good else 'FAIL'))
    if not good:
        print('   out: %s' % out2.strip()[:300])
    ok &= good

    # ③ 对照: 去掉旧语法声明
    src = open(OLD, encoding='utf-8', errors='replace').read()
    lines = [('// ' + l if l.strip().startswith('泛型 ') else l) for l in src.split('\n')]
    ctrl = os.path.join(d, 'ctrl.c')
    open(ctrl, 'w', encoding='utf-8').write('\n'.join(lines))
    r = run(a.cc, ctrl, os.path.join(d, 'ctrl.exe'))
    out3 = (r.stdout or '') + (r.stderr or '')
    # 去掉声明后 取大 未定义 -> 允许 rc!=0, 但**不得**再有 W9601
    good = ('W9601' not in out3)
    print('③ 对照(无旧语法) W9601=%s rc=%d -> %s' % ('W9601' in out3, r.returncode, 'PASS' if good else 'FAIL'))
    ok &= good

    print('=== S1/W9601 判据: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
