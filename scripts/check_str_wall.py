# -*- coding: utf-8 -*-
"""check_str_wall —— **字符串字面量墙**（台账 D53 · 2026-10-25 启元审计递刀）。

现场（审计 `D:\\_opt_proto\\_str3\\`，编译器 `199724E7`）：
  · `m13500_10.jy`（13,500 条 × ~10 字节，源 162 KB）  ⟹ rc=0
  · `m13500_20.jy`（13,500 条 × ~18 字节，源 297 KB）  ⟹ **rc=0xC0000005（静默段错误，无任何诊断）**

本轮（R830）已做「便宜的一半」：词法 在 字面量收尾处 累计 (条数, 字节)，
越过**实测边界**（条数 > 12,000 且 字节 > 140,000）⟹ 响亮 A6112 拒绝 + 退出(1)，
不再段错误。实测边界：13,500×10 (=135 KB) 可编；13,500×11 (=148 KB) / 13,500×18 (=243 KB) 崩；
9,000×20 (=180 KB) 可编。

本判据三条（全过才 rc=0）：
  ① 145 KB 正例：`-o` rc=0（不许因配额误伤）
  ② 297 KB 反例：rc≠0 **且** 输出含诊断（不许再是 0xC0000005 静默）
  ③ 297 KB 真编过：rc=0  ← **根因未定位，本轮仍红**（故台账 D53 保持 blocked）

用法: python scripts/check_str_wall.py [--cc qcc_zh_seed.exe]
退出码: 0 = 三条全过（墙已根治）；1 = 有红（②③ 之外的红都算缺陷）；2 = 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CRASH = (-1073741819, 3221225477)


def gen(path, n, per):
    items = ['"%s"' % (('s%06d' % i) + 'x' * max(0, per - 7)) for i in range(n)]
    src = ('\ufeff#包含 <stdio.h>\n静 字 *街[%d] = { ' % n) + ', '.join(items) + \
          ' };\n静 整 主(空) { printf("%s\\n", 街[0]); 返 0; }\n'
    io.open(path, 'w', encoding='utf-8', newline='\n').write(src)
    return os.path.getsize(path)


def run(cc, src, out):
    r = subprocess.run([cc, src, '-o', out], capture_output=True, timeout=1800)
    msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    return r.returncode, msg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_strwall_')
    # ????????????????????????????????8 / 18 ?????
    live1 = r'D:\\_opt_proto\\_str3\\m13500_10.jy'
    live2 = r'D:\\_opt_proto\\_str3\\m13500_20.jy'
    if os.path.exists(live1) and os.path.exists(live2):
        s145, s297 = live1, live2
        n1, n2 = os.path.getsize(s145), os.path.getsize(s297)
        print('[src] ????????D:\\_opt_proto\\_str3?')
    else:
        s145 = os.path.join(w, 'm13500_10.c')
        s297 = os.path.join(w, 'm13500_20.c')
        n1 = gen(s145, 13500, 8)
        n2 = gen(s297, 13500, 18)
        print('[src] ???? ? ?????8 / 18 ?????')
    rc1, m1 = run(cc, s145, os.path.join(w, 'a.exe'))
    print('[145KB 正例 ] rc=%d %s' % (rc1, 'OK' if rc1 == 0 else 'FAIL | ' + m1.strip()[:120]))
    rc2, m2 = run(cc, s297, os.path.join(w, 'b.exe'))
    crash = rc2 in CRASH
    loud = (rc2 != 0) and (not crash) and ('字面' in m2 or 'A6112' in m2)
    print('[297KB 反例 ] rc=%d %s' % (rc2, 'OK(响亮拒绝)' if loud else ('SILENT-CRASH' if crash else 'UNEXPECTED')))
    if loud:
        for l in m2.strip().split('\n')[:2]:
            print('    ' + l.strip()[:150])
    rc3 = rc2  # 真编过 = 反例 rc==0
    print('[297KB 真编过] %s（根因未定位 ⟹ 本轮预期红）' % ('OK' if rc3 == 0 else 'RED'))
    ok = (rc1 == 0) and loud and (rc3 == 0)
    print('=== 字符串字面量墙（%d/%d 字节源）: %s ==='
          % (n1, n2, 'PASS' if ok else ('FAIL（便宜的一半=%s，真编过=%s）'
                                        % ('OK' if loud else 'RED', 'OK' if rc3 == 0 else 'RED'))))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
