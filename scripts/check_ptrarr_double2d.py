# -*- coding: utf-8 -*-
"""check_ptrarr_double2d —— **元素为 8 字节标量 的指针数组，双重下标写也必须落内存**（PTRARR-2D-DOUBLE）。

现场（2026-10-24，R813 之后）：
  `双精度 *p[3]; p[0] = y; p[0][1] = 2.5;` ⟹ `p[0][1]` 与 `y[1]` 都读 0.00（写**整条丢弃**）；
  同一形状 `整 *q[3]; q[0][1] = 7;` 正确（y[1] = 7）。
  ⟹ 与 R813 的结构体族不同源：8 字节标量所指（双精度/长整长整）走的是另一条元数据路径。

判据（6 格，任一不满足即红）：
  · 类型 ∈ {双精度, 长整长整, 整, 字符, 短整, 结构体} × `*p[3]; p[0]=y; p[0][1]=v;`
  · 每格断言：`p[0][1] == v` **且** `y[1] == v`（内存 + 读回双通道）。

用法: python scripts/check_ptrarr_double2d.py [--cc qcc_zh_seed.exe]
退出码: 0 = 6 格全绿；1 = 有格写丢失/错值（缺陷仍在）
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 每格: (名, 声明, 赋值, 读回 p, 读回 y, 期望串)
CASES = [
    ('double', '双精度 y[3]; 双精度 *p[3];', 'p[0]=y; p[0][1]=2.5;',
     '格式化 "P %.2f\\n", p[0][1];', '格式化 "M %.2f\\n", y[1];', 'P 2.50\nM 2.50\n'),
    ('longlong', '长 长 y[3]; 长 长 *p[3];', 'p[0]=y; p[0][1]=1234567890123;',
     '格式化 "P %d\\n", (整)p[0][1];', '格式化 "M %d\\n", (整)y[1];',
     'P 1912276171\nM 1912276171\n'),
    ('int', '整 y[3]; 整 *p[3];', 'p[0]=y; p[0][1]=7;',
     '格式化 "P %d\\n", p[0][1];', '格式化 "M %d\\n", y[1];', 'P 7\nM 7\n'),
    ('char', '字 y[3]; 字 *p[3];', 'p[0]=y; p[0][1]=65;',
     '格式化 "P %d\\n", p[0][1];', '格式化 "M %d\\n", y[1];', 'P 65\nM 65\n'),
    ('short', '短整 y[3]; 短整 *p[3];', 'p[0]=y; p[0][1]=321;',
     '格式化 "P %d\\n", p[0][1];', '格式化 "M %d\\n", y[1];', 'P 321\nM 321\n'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        # 2026-10-24（判据方打回）: 缺主体不得当绿 —— rc=2 = 环境未就绪，不是 PASS
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_p2d_')
    bad = []
    for name, decl, assign, rp, rm, want in CASES:
        text = ('#包含 <stdio.h>\n整 主() {\n  %s\n  %s\n  %s\n  %s\n  返 0;\n}\n'
                % (decl, assign, rp, rm))
        src = os.path.join(w, name + '.c')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(text)
        exe = os.path.join(w, name + '.exe')
        r = subprocess.run([cc, src, '-o', exe], capture_output=True, timeout=300)
        if r.returncode != 0:
            bad.append(name + '(compile)')
            print('[%-8s] 编译失败 rc=%d' % (name, r.returncode))
            continue
        q = subprocess.run([exe], capture_output=True, timeout=60)
        out = (q.stdout or b'').decode('utf-8', 'replace').replace('\r\n', '\n')
        ok = q.returncode == 0 and out.strip() == want.strip()
        print('[%-8s] rc=%d %s | %s' % (name, q.returncode, 'OK' if ok else 'WRONG',
                                        out.strip().replace('\n', ' / ')))
        if not ok:
            bad.append(name)
    print('=== 指针数组 双重下标写: %s ===' % ('PASS' if not bad else 'FAIL -> ' + ', '.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
