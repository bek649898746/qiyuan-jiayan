#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_mech8_verdict —— 8 项机制「真假」判定判据（ORD-20261027-10 · 施工单《8 项机制真假核验》）

判定口径（施工单 §三）四档：✅真接入 / 🔴表层 / ❌未实现 / 定案。
本判据对**6 项有运行时语义**的机制跑真探针（正例证明真的生效），对**2 项**给出「由既有机制承担」的
静态证据（本语言没有独立的运行时对象）。任一探针行为与记录不符 ⟹ 红。

| # | 机制 | 判定 | 证据 |
|--:|:--|:--|:--|
| 33 | 借用分裂 | ✅真接入 | `可变借用 s.x` 后 s.y 可读写（探针）；主源 `串弊()` 路径级借用 |
| 82 | 错误类型系统 | ✅真接入 | `错误 整 f()` + `错 N` + `尝试` 传播（探针 7/5/7；A6004 载荷类型检查） |
| 233 | 位域深化 | ✅真接入 | `无符号 整 a : 3` 位宽生效（探针 5/17，结构体 4 字节） |
| 68 | 背压 | ✅真接入 | `背压 送` + `就绪送()` 满时返 0（探针 1/2/0） |
| 210 | 高阶/函数式 | ✅真接入 | `折叠 数组, n, 累加器, 函` 编译成循环（探针 6） |
| 218 | Maybe/Result 链 | ✅真接入 | `可空` + `?.` 可选链（探针 9/0） |
| 130 | 类型相等判定 | 定案 | 由「型序（整数身份）+ 类型表唯一登记」承担，无运行时类型对象 |
| 237 | 胖指针 | 定案 | 由「数组维度元数据（数组尺寸/域行数）+ `切片` 内建」承担，无 (ptr,len) 运行时对 |

用法：python scripts/check_mech8_verdict.py --cc qcc_zh_v1.exe
"""
from qcc_tmp import tmp_dir, tmp_root
import argparse
import io
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RUNTIME = [
    ("33-借用分裂", '''# 包含 <stdio.h>
构 点 { 整 x; 整 y; };
函数 整 主(空) {
    构 点 s; s.x = 5; s.y = 6;
    可变借用 s.x;
    s.y = 9;
    格式化 "%d\\n", s.y;
    放还 s.x;
    格式化 "%d %d\\n", s.x, s.y;
    返 0;
}
''', "9\n5 9\n"),
    ("82-错误类型系统", '''# 包含 <stdio.h>
错误 整 取(整 k) { 若 (k < 0) 错 7; 返 k; }
错误 整 中(整 k) { 尝试 取(k); 返 0; }
函数 整 主(空) {
    错误 整 甲 = 取(-1);
    错误 整 乙 = 取(5);
    错误 整 丙 = 中(-1);
    若 (甲 != 7 || 乙 != 5 || 丙 != 7) 返 1;
    格式化 "%d %d %d\\n", 甲, 乙, 丙;
    返 0;
}
''', "7 5 7\n"),
    ("68-背压", '''# 包含 <stdio.h>
函数 整 主(空) {
    通道 q, 2;
    送 q, 1;
    背压 送 q, 2;
    整 满 = 就绪送(q);
    整 a = 0; 整 b = 0;
    收 q, a; 收 q, b;
    格式化 "%d %d %d\\n", a, b, 满;
    返 0;
}
''', "1 2 0\n"),
    ("210-高阶", '''# 包含 <stdio.h>
函数 整 加(整 a, 整 b) { 返 a + b; }
函数 整 主(空) {
    整 a[3]; a[0] = 1; a[1] = 2; a[2] = 3;
    整 s = 0;
    折叠 a, 3, s, 加;
    格式化 "%d\\n", s;
    返 0;
}
''', "6\n"),
    ("218-Maybe链", '''# 包含 <stdio.h>
构 节 { 整 v; };
函数 整 主(空) {
    构 节 b; b.v = 9;
    构 节 *p = &b;
    可空 构 节 *q = 无;
    格式化 "%d %d\\n", p?.v, q?.v;
    返 0;
}
''', "9 0\n"),
    ("233-位域", '''# 包含 <stdio.h>
构 位 { 无符号 整 a : 3; 无符号 整 b : 5; };
函数 整 主(空) {
    构 位 v; v.a = 5; v.b = 17;
    格式化 "%d %d %d\\n", (整)v.a, (整)v.b, (整)大小(v);
    返 0;
}
''', "5 17 4\n"),
]

# 定案两项：由既有机制承担 —— 静态证据（主源里必须能找到承担者）
SUBSTITUTE = [
    ("130-类型相等判定", ["类型表[", "型序"]),
    ("237-胖指针", ["数组尺寸", "域行数", "切片"]),
]


def run(cmd, timeout=300):
    p = subprocess.run(cmd, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=timeout)
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 找不到编译器: %s' % cc)
        return 2

    src_all = io.open(os.path.join(REPO, 'srclib_jiayan', 'qcc_zh.jy'),
                      encoding='utf-8', errors='replace').read()
    tmp = tmp_dir('mech8')
    bad = []
    try:
        print('%-18s %-10s %s' % ('机制', '判定', '观测'))
        for name, src, want in RUNTIME:
            f = os.path.join(tmp, 'm.jy')
            exe = os.path.join(tmp, 'm.exe')
            if os.path.exists(exe):
                os.remove(exe)
            io.open(f, 'w', encoding='utf-8', newline='\n').write(src)
            rc, out = run([cc, f, '-o', exe])
            if rc != 0 or not os.path.exists(exe):
                bad.append('%s: 编译失败 rc=%d %s' % (name, rc, out.strip()[:120]))
                print('%-18s %-10s %s' % (name, '?', '编译失败'))
                continue
            rrc, rout = run([exe])
            ok = (rrc == 0 and rout == want)
            print('%-18s %-10s %r' % (name, '✅真接入' if ok else '🔴不符', rout))
            if not ok:
                bad.append('%s: 期望 %r 实得 %r (rc=%d)' % (name, want, rout, rrc))
        for name, keys in SUBSTITUTE:
            miss = [k for k in keys if k not in src_all]
            ok = not miss
            print('%-18s %-10s %s' % (name, '定案' if ok else '?', ('承担者: ' + '/'.join(keys)) if ok else ('缺: ' + ','.join(miss))))
            if not ok:
                bad.append('%s: 承担机制缺证据 %s' % (name, miss))
        if bad:
            print('[FAIL] 8 项判定与记录不符 %d 处' % len(bad))
            for b in bad[:8]:
                print('   - ' + b)
            return 1
        print('[PASS] 8 项机制判定：6 项真接入（探针实测）+ 2 项定案（承担者已在主源）')
        return 0
    finally:
        if not a.keep:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
