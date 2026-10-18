# -*- coding: utf-8 -*-
"""D-DF 数据流求值顺序判据（2026-10-18）。

① 正例：连续 `数据流` 纯算术声明，前向引用经拓扑重排后运行
   `DFORDER 2 1 3 2 1`（甲→乙→丙；丁→戊）。
② 负例：初始化含函数调用（非纯）⟹ **不重排**，前向引用仍 A1001
   （证明不静默改副作用顺序）。
③ 无初值：`数据流 整 甲;` 必须 **快速** A6089（skeptic 2026-10-18：原 `流析` 首失败
   不前进 ⟹ 扫描死循环；本判据带 15s 超时，死循环即 FAIL）。
用法: python scripts/check_dataflow_order.py --cc qcc_zh_v2.exe
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
函数 整 主(空) {
    数据流 整 乙 = 甲 + 1;
    数据流 整 甲 = 1;
    数据流 整 丙 = 乙 + 1;
    数据流 整 戊 = 丁 + 1;
    数据流 整 丁 = 1;
    格式化 "DFORDER %d %d %d %d %d\n", 乙, 甲, 丙, 戊, 丁;
    返 0;
}
'''

NEG = r'''#包含 <stdio.h>
函数 整 戊(空) { 返 1; }
函数 整 主(空) {
    数据流 整 乙 = 戊() + 甲;
    数据流 整 甲 = 1;
    返 乙;
}
'''

NOINIT = r'''#包含 <stdio.h>
函数 整 主(空) {
    数据流 整 甲;
    返 0;
}
'''


def run(args, timeout=180):
    return subprocess.run(args, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', timeout=timeout, cwd=REPO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    a = ap.parse_args()
    w = tempfile.mkdtemp(prefix='qcc_df_order_')
    fails = []

    pos = os.path.join(w, 'pos.c')
    exe = os.path.join(w, 'pos.exe')
    open(pos, 'w', encoding='utf-8', newline='\n').write(POS)
    r = run([a.cc, pos, '-o', exe])
    if r.returncode != 0:
        fails.append('正例编译失败 rc=%d %s%s' % (r.returncode, r.stdout, r.stderr))
    else:
        q = run([exe])
        if q.returncode != 0 or 'DFORDER 2 1 3 2 1' not in (q.stdout or ''):
            fails.append('正例运行/值失败 rc=%d out=%r' % (q.returncode, q.stdout))

    neg = os.path.join(w, 'neg.c')
    open(neg, 'w', encoding='utf-8', newline='\n').write(NEG)
    rn = run([a.cc, neg, '-o', os.path.join(w, 'neg.exe')])
    if rn.returncode == 0:
        fails.append('负例（不纯前向引用）未被拒绝 rc=0（可能被静默重排）')
    elif 'A1001' not in ((rn.stdout or '') + (rn.stderr or '')):
        fails.append('负例未报 A1001: %s%s' % (rn.stdout, rn.stderr))

    noinit = os.path.join(w, 'noinit.c')
    open(noinit, 'w', encoding='utf-8', newline='\n').write(NOINIT)
    try:
        r0 = run([a.cc, noinit, '-o', os.path.join(w, 'noinit.exe')], timeout=15)
        if r0.returncode == 0:
            fails.append('无初值 数据流 未被拒绝 rc=0')
        elif 'A6089' not in ((r0.stdout or '') + (r0.stderr or '')):
            fails.append('无初值 数据流 未报 A6089: %s%s' % (r0.stdout, r0.stderr))
    except subprocess.TimeoutExpired:
        fails.append('无初值 数据流 编译超时（扫描死循环回归）')

    print('== D-DF 数据流求值顺序 ==')
    print('positive reorder/run: %s' % ('OK' if not any('正例' in f for f in fails) else 'FAIL'))
    print('impure stays loud A1001: %s' % ('OK' if not any('负例' in f for f in fails) else 'FAIL'))
    print('no-init loud A6089 (15s): %s' % ('OK' if not any('无初值' in f for f in fails) else 'FAIL'))
    if fails:
        print('FAIL=%d' % len(fails))
        for f in fails:
            print('  - ' + f)
        print('work=' + w)
        return 1
    print('=== D-DF dataflow order: PASS ===')
    shutil.rmtree(w, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
