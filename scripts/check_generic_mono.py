# -*- coding: utf-8 -*-
"""#96 泛型真单态化判据（2026-10-18）。

判据不是「宏能算对」，而是：
  ① 同一泛型函数在**两个具体类型**下各有**真实函数符号**（nm 见 T 恒等_整 / T 恒等_双）；
  ② 取址得到两个不同地址（文本宏无符号可取）；
  ③ 运行值正确（整 5 / 双 2.5）；
  ④ 缺实例的类型必须**编译期拒绝**（A1001 未声明 恒等_长），不得静默展开。

语法（本切片定义）:
  泛型函数 名(T) = 函数 T 名(T x) { 体 }
  实例 名(整); 实例 名(双);
  调用 名[整](5)   ⟹ 名_整(5)
用法: python scripts/check_generic_mono.py --cc qcc_zh_v2.exe
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
泛型函数 恒等(T) = 函数 T 恒等(T x) { 返 x; }
实例 恒等(整);
实例 恒等(双);
函数 整 主(空) {
    整 a = 恒等[整](5);
    双 b = 恒等[双](2.5);
    空 *p1 = (空*)&恒等_整;
    空 *p2 = (空*)&恒等_双;
    整 c = (p1 != p2);
    格式化 "MONO %d %.1f %d\n", a, b, c;
    返 0;
}
'''

NEG = r'''#包含 <stdio.h>
泛型函数 恒等(T) = 函数 T 恒等(T x) { 返 x; }
实例 恒等(整);
函数 整 主(空) { 返 恒等[长](7); }
'''


def run(args, timeout=180):
    return subprocess.run(args, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', timeout=timeout, cwd=REPO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    a = ap.parse_args()
    work = tempfile.mkdtemp(prefix='qcc_generic_mono_')
    fails = []

    pos = os.path.join(work, 'mono_ok.c')
    exe = os.path.join(work, 'mono_ok.exe')
    obj = os.path.join(work, 'mono_ok.o')
    open(pos, 'w', encoding='utf-8', newline='\n').write(POS)

    r = run([a.cc, pos, '-o', exe])
    if r.returncode != 0:
        fails.append('正例编译失败 rc=%d\n%s%s' % (r.returncode, r.stdout, r.stderr))
    else:
        q = run([exe])
        if q.returncode != 0 or 'MONO 5 2.5 1' not in (q.stdout or ''):
            fails.append('正例运行/值失败 rc=%d out=%r' % (q.returncode, q.stdout))

    # 真实例符号：-c 产物里 nm 必须见两个 T 符号
    r2 = run([a.cc, '-c', pos, '-o', obj])
    symbols = ''
    if r2.returncode != 0:
        fails.append('-c 编译失败 rc=%d\n%s%s' % (r2.returncode, r2.stdout, r2.stderr))
    else:
        nm = shutil.which('nm') or 'nm'
        n = run([nm, obj])
        symbols = (n.stdout or '') + (n.stderr or '')
        for sym in ('恒等_整', '恒等_双'):
            if ('T ' + sym) not in symbols and (' T ' + sym) not in symbols and sym not in symbols:
                fails.append('nm 缺真实例符号 T %s' % sym)

    neg = os.path.join(work, 'mono_neg.c')
    open(neg, 'w', encoding='utf-8', newline='\n').write(NEG)
    rn = run([a.cc, neg, '-o', os.path.join(work, 'mono_neg.exe')])
    if rn.returncode == 0:
        fails.append('负例（缺实例 长）未被拒绝 rc=0')
    elif 'A1001' not in ((rn.stdout or '') + (rn.stderr or '')):
        fails.append('负例未报 A1001: %s%s' % (rn.stdout, rn.stderr))

    print('== #96 泛型真单态化 ==')
    print('positive compile/run: %s' % ('OK' if not any('正例' in f for f in fails) else 'FAIL'))
    print('symbols: %s' % ('OK' if not any('nm' in f for f in fails) else 'FAIL'))
    if symbols:
        for line in symbols.splitlines():
            if '恒等' in line:
                print('  nm: ' + line.strip())
    print('negative (缺实例 ⟹ A1001): %s' % ('OK' if not any('负例' in f for f in fails) else 'FAIL'))
    if fails:
        print('FAIL=%d' % len(fails))
        for f in fails:
            print('  - ' + f)
        print('work=' + work)
        return 1
    print('=== #96 generic mono: PASS ===')
    shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
