# -*- coding: utf-8 -*-
"""#96 表达式泛型（G-96b）真单态化判据（2026-10-18）。

① 正例：`泛型函数 取大(T) = 函数 T 取大(T x,T y){返 expr;}` + 实例 整/双
   ⟹ 运行 `MONOEXPR 7 7.5`；`nm` 见 `T 取大_整` / `T 取大_双`。
② 负例：缺实例 `长` ⟹ A1001。
③ 判别：旧 `泛型 取大(x,y)=expr;` 宏路径 `-c` 编译 rc=0，但 nm **无** `取大_整`
   ⟹ 文本宏路径不可能冒充真单态化。
④ 字面量边界（skeptic 2026-10-18）：泛型体里的字符串必须逐字保留 ——
   `"T=%d"` 不得变成 `"整=%d"`；字符串里的 `{` 不得计入模板体花括号深度。
   锚文件 `feat_zh_generic_string_ok.c` / `feat_zh_generic_brace_str_ok.c`（RED 现场见
   `_evidence/20261018_round8/generic_literal_red.log`）。
用法: python scripts/check_generic_expr_mono.py --cc qcc_zh_v2.exe
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

POS = r'''#包含 <stdio.h>
泛型函数 取大(T) = 函数 T 取大(T x, T y) { 返 ((x) > (y) ? (x) : (y)); }
实例 取大(整);
实例 取大(双);
函数 整 主(空) {
    格式化 "MONOEXPR %d %.1f\n", 取大[整](3, 7), 取大[双](3.5, 7.5);
    返 0;
}
'''

NEG = r'''#包含 <stdio.h>
泛型函数 取大(T) = 函数 T 取大(T x, T y) { 返 ((x) > (y) ? (x) : (y)); }
实例 取大(整);
函数 整 主(空) { 返 取大[长](7, 9); }
'''

MACRO = r'''#包含 <stdio.h>
泛型 取大(x, y) = ((x) > (y) ? (x) : (y));
函数 整 主(空) { 返 取大(3, 7); }
'''


def run(args, timeout=180):
    return subprocess.run(args, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', timeout=timeout, cwd=REPO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    a = ap.parse_args()
    w = tempfile.mkdtemp(prefix='qcc_expr_mono_')
    fails = []
    nm = shutil.which('nm') or 'nm'

    pos = os.path.join(w, 'pos.c')
    exe = os.path.join(w, 'pos.exe')
    obj = os.path.join(w, 'pos.o')
    open(pos, 'w', encoding='utf-8', newline='\n').write(POS)
    r = run([a.cc, pos, '-o', exe])
    if r.returncode != 0:
        fails.append('正例编译失败 rc=%d %s%s' % (r.returncode, r.stdout, r.stderr))
    else:
        q = run([exe])
        if q.returncode != 0 or 'MONOEXPR 7 7.5' not in (q.stdout or ''):
            fails.append('正例运行/值失败 rc=%d out=%r' % (q.returncode, q.stdout))
    syms = ''
    r2 = run([a.cc, '-c', pos, '-o', obj])
    if r2.returncode != 0:
        fails.append('-c 失败 rc=%d %s%s' % (r2.returncode, r2.stdout, r2.stderr))
    else:
        n = run([nm, obj])
        syms = (n.stdout or '') + (n.stderr or '')
        for s in ('取大_整', '取大_双'):
            if ('T ' + s) not in syms and (' T ' + s) not in syms:
                fails.append('nm 缺真实例符号 T %s' % s)

    neg = os.path.join(w, 'neg.c')
    open(neg, 'w', encoding='utf-8', newline='\n').write(NEG)
    rn = run([a.cc, neg, '-o', os.path.join(w, 'neg.exe')])
    if rn.returncode == 0:
        fails.append('负例（缺实例 长）未被拒绝 rc=0')
    elif 'A1001' not in ((rn.stdout or '') + (rn.stderr or '')):
        fails.append('负例未报 A1001: %s%s' % (rn.stdout, rn.stderr))

    mac = os.path.join(w, 'mac.c')
    mobj = os.path.join(w, 'mac.o')
    open(mac, 'w', encoding='utf-8', newline='\n').write(MACRO)
    rm = run([a.cc, '-c', mac, '-o', mobj])
    macro_sym = '?'
    if rm.returncode != 0:
        fails.append('宏路径 -c 失败 rc=%d' % rm.returncode)
    else:
        mn = run([nm, mobj])
        ms = (mn.stdout or '') + (mn.stderr or '')
        macro_sym = 'PRESENT' if ('T 取大_整' in ms or ' T 取大_整' in ms) else 'ABSENT'
        if macro_sym != 'ABSENT':
            fails.append('宏路径意外出现 取大_整（判别失效）')

    # ④ 字面量边界：字符串内容不参与类型形参替换 / 花括号深度统计
    lit_ok = brace_ok = True
    for name, anchor, want in [
        ('literal', 'feat_zh_generic_string_ok.c', 'LITSTR T=7'),
        ('brace-in-string', 'feat_zh_generic_brace_str_ok.c', 'BRACE {7'),
    ]:
        src = os.path.join(REPO, 'scripts', anchor)
        if not os.path.exists(src):
            fails.append('%s 锚缺失: %s' % (name, src))
            if name == 'literal':
                lit_ok = False
            else:
                brace_ok = False
            continue
        ex = os.path.join(w, name.replace('-', '_') + '.exe')
        rr = run([a.cc, src, '-o', ex])
        if rr.returncode != 0:
            fails.append('%s 编译失败 rc=%d %s%s' % (name, rr.returncode, rr.stdout, rr.stderr))
            if name == 'literal':
                lit_ok = False
            else:
                brace_ok = False
            continue
        qq = run([ex])
        if qq.returncode != 0 or want not in (qq.stdout or ''):
            fails.append('%s 运行/值失败 rc=%d out=%r（应含 %s）' % (
                name, qq.returncode, qq.stdout, want))
            if name == 'literal':
                lit_ok = False
            else:
                brace_ok = False

    print('== #96 表达式泛型真单态化 ==')
    print('positive run: %s' % ('OK' if not any('正例' in f for f in fails) else 'FAIL'))
    print('real symbols: %s' % ('OK' if not any('nm' in f for f in fails) else 'FAIL'))
    for line in syms.splitlines():
        if '取大' in line:
            print('  nm: ' + line.strip())
    print('negative (缺实例 A1001): %s' % ('OK' if not any('负例' in f for f in fails) else 'FAIL'))
    print('macro path symbol 取大_整: %s（应为 ABSENT）' % macro_sym)
    print('literal string preserved: %s' % ('OK' if lit_ok else 'FAIL'))
    print('brace in string skipped: %s' % ('OK' if brace_ok else 'FAIL'))
    if fails:
        print('FAIL=%d' % len(fails))
        for f in fails:
            print('  - ' + f)
        print('work=' + w)
        return 1
    print('=== #96 expr mono: PASS ===')
    shutil.rmtree(w, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
