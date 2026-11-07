#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_chan_move —— ORD-20261104-105 ①：送通道 = 移动（非 Copy）。

四例（工程判据，不是语法糖自证）：
  1. 结构体发送后读字段      ⟹ A3101（已移）
  2. 结构体发送后重新赋值再读 ⟹ 通过
  3. 标量发送后读（负例）     ⟹ 通过（Copy 豁免）
  4. 临时值发送              ⟹ 通过

用法: python scripts/check_chan_move.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HEAD = '#包含 <stdio.h>\n构 盒 { 整 通; };\n'

CASES = [
    ('struct_send_then_read', HEAD + '''函数 整 主(空) {
    整 q = 通道新建(2);
    盒 b;
    b.通 = 7;
    送 q, b;
    整 x = b.通;
    释放通道(q);
    返 x;
}
''', 'A3101'),
    ('struct_send_reassign_ok', HEAD + '''函数 整 主(空) {
    整 q = 通道新建(2);
    盒 b;
    b.通 = 7;
    送 q, b;
    b.通 = 9;
    整 x = b.通;
    释放通道(q);
    返 x;
}
''', None),
    ('scalar_send_read_ok', HEAD + '''函数 整 主(空) {
    整 q = 通道新建(2);
    整 x = 42;
    送 q, x;
    整 y = x;
    释放通道(q);
    返 y;
}
''', None),
    ('temp_send_ok', HEAD + '''函数 整 主(空) {
    整 q = 通道新建(2);
    送 q, 41 + 1;
    释放通道(q);
    返 0;
}
''', None),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    red = []
    with tempfile.TemporaryDirectory(prefix='qcc_chanmove_') as tmp:
        for name, src, want in CASES:
            p = os.path.join(tmp, name + '.c')
            open(p, 'w', encoding='utf-8').write(src)
            r = subprocess.run([cc, p, '-o', os.path.join(tmp, name + '.exe')], cwd=ROOT,
                               capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
            txt = (r.stdout or '') + (r.stderr or '')
            has = (want is not None) and (want in txt)
            ok = (r.returncode != 0 and has) if want else (r.returncode == 0)
            print('[%s] %-28s rc=%d want=%s' % ('PASS' if ok else 'FAIL', name, r.returncode, want or 'ok'))
            if not ok:
                print('       ' + txt.strip()[:300])
                red.append(name)
    if red:
        print('=== 通道 move: FAIL -> %s ===' % red)
        return 1
    print('=== 通道 move: PASS（4/4）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
