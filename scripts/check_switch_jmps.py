#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_switch_jmps —— ORD-20261108-158 判据：`switch` 的 `break` 不许生成 O(n²) 跳转链。

施工单 `_施工单_switch的break跨层跳出生成O_n平方跳转链_根因闭环_启元_20261108.md` §八：
  J1  N=80 的 `jmp` 数 ≤ 100（现状 3,329 = 理论 n²/2 的 1.04 倍）
  J3  N 从 20 翻到 80，jmp 增长 ≤ 2.2×（现状 3.80×）
  J4  `je` / `sete` 数不回归（条件判断本身是正常的，别把修法做成「少生成条件」）

做法：同构 case（每 case 体 `{ int ra = N; r = ra + 1; break; }`），
`-c` 编成 obj，用 objdump 数 `jmp` / `je` / `sete`。数字与施工单逐项对得上才说明量具正确。

用法: python scripts/check_switch_jmps.py [--cc qcc_zh_v1.exe]
退出码: 0 = PASS（已是跳转表/单出口）；1 = 仍是 O(n²)
"""
import argparse
import io
import os
import re
import subprocess
import time
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from qcc_tmp import tmp_dir  # noqa: E402

JMP_MAX = 100       # J1
RATIO_MAX = 2.2     # J3


def gen_switch(n):
    parts = ['int f(int x) { int r = 0; switch (x) {']
    for i in range(n):
        parts.append('  case %d: { int ra = %d; r = ra + 1; break; }' % (i, i))
    parts.append('} return r; }')
    parts.append('int main(void) { return f(3); }')
    return '\n'.join(parts) + '\n'


def run(cmd, timeout=600):
    r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                       errors='replace', timeout=timeout)
    return r.returncode, (r.stdout or '') + (r.stderr or '')


def measure(cc, n, work):
    src = os.path.join(work, 'T%d.c' % n)
    obj = os.path.join(work, 'T%d.obj' % n)
    io.open(src, 'w', encoding='utf-8').write(gen_switch(n))
    rc, out = run([cc, src, '-c', '-o', obj])
    if rc != 0 or not os.path.exists(obj):
        print('[ERR] N=%d 编译失败 rc=%d\n%s' % (n, rc, out[-400:]))
        return None
    m = re.search(r'\((\d+) bytes code', out)
    text = int(m.group(1)) if m else -1
    rc, dis = run(['objdump', '-d', obj])
    if rc != 0:
        print('[ERR] objdump 失败 rc=%d' % rc)
        return None
    jmp = len(re.findall(r'^\s+[0-9a-f]+:.*\sjmp\s', dis, re.M))
    je = len(re.findall(r'^\s+[0-9a-f]+:.*\sje\s', dis, re.M))
    sete = len(re.findall(r'^\s+[0-9a-f]+:.*\ssete\s', dis, re.M))
    return {'n': n, 'text': text, 'jmp': jmp, 'je': je, 'sete': sete}


RUN_SRC = """int f(int x) {
  int r = 0;
  switch (x) {
{body}
  }
  return r;
}
int main(void) {
  long s = 0;
  for (int i = 0; i < 3000000; i++) s = s + f(i % 40);
  return (s == 123456789) ? 1 : 0;   /* 恒 0（只用 s 防 DCE），避免退出码被当失败 */
}
"""


def gen_run(n):
    body = '\n'.join('    case %d: { int ra = %d; r = ra + 1; break; }' % (i, i) for i in range(n))
    return RUN_SRC.replace('{body}', body)


def time_exe(cc, work, tag, n=40, reps=5):
    """编运行探针并跑 reps 次 → (最小秒, 产物字节, 全部样本秒)。

    量具声明：取 **最小** 而非中位 —— 计时噪声只会拖慢、不会加快，故 min 是新旧两边的
    共同下界估计；比值仍可比（两边同样受噪声影响，噪声只会让偏离变小，不会造假通过）。
    """
    src = os.path.join(work, 'R_%s.c' % tag)
    exe = os.path.join(work, 'R_%s.exe' % tag)
    io.open(src, 'w', encoding='utf-8').write(gen_run(n))
    rc, out = run([cc, src, '-o', exe], timeout=600)
    if rc != 0 or not os.path.exists(exe):
        print('[ERR] J7 运行探针 编译失败 cc=%s rc=%d\n%s' % (os.path.basename(cc), rc, out[-300:]))
        return None
    ts = []
    for _ in range(reps):
        t0 = time.perf_counter()
        r = subprocess.run([exe], capture_output=True, timeout=120)
        ts.append(time.perf_counter() - t0)
        if r.returncode != 0:
            print('[ERR] J7 运行失败 rc=%d' % r.returncode)
            return None
    return min(ts), os.path.getsize(exe), ts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--old-cc', default=None, help='J7 A/B 的旧编译器（缺省 = 跳过 J7 并 显式 说明）')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(os.path.dirname(HERE), a.cc)
    if not os.path.exists(cc):
        print('[ERR] 缺件: %s' % cc)
        return 2
    work = tmp_dir('switch_jmp')
    ms = [measure(cc, n, work) for n in (20, 40, 80)]
    if any(x is None for x in ms):
        return 2
    m20, m40, m80 = ms
    for x in ms:
        print('N=%-3d .text=%-8d jmp=%-5d je=%-4d sete=%-4d'
              % (x['n'], x['text'], x['jmp'], x['je'], x['sete']))
    # J3 的口径是「n **翻倍** 时代码增长 ≤ 2.2×」（施工单原表：3.50× / 3.80×）。
    # 用 20→40、40→80 两次翻倍各自判，而不是 20→80 的四倍跨度
    # （后者即使线性增长也必然 4×，写成 2.2× ⟹ 修好了也永远绿不了）。
    r1 = m40['text'] / m20['text'] if m20['text'] else 0.0
    r2 = m80['text'] / m40['text'] if m40['text'] else 0.0
    print('每翻倍代码增长: 20->40 %.2fx · 40->80 %.2fx（上限 %.1fx）' % (r1, r2, RATIO_MAX))
    bad = []
    if m80['jmp'] > JMP_MAX:
        bad.append('J1: N=80 jmp=%d > %d' % (m80['jmp'], JMP_MAX))
    if r1 > RATIO_MAX:
        bad.append('J3: 20->40 增长 %.2fx > %.1fx' % (r1, RATIO_MAX))
    if r2 > RATIO_MAX:
        bad.append('J3: 40->80 增长 %.2fx > %.1fx' % (r2, RATIO_MAX))
    # J4：条件判断必须仍然是「每 case 一个 je/sete」——修法不许靠少生成条件来降 jmp
    if m80['je'] < 70 or m80['sete'] < 70:
        bad.append('J4: je=%d / sete=%d（应 ≈80/80，条件生成回归）' % (m80['je'], m80['sete']))
    # J7（ORD-158）: **运行性能 不退化** —— 同一运行探针（300 万次 f() 调用）用 旧/新 编译器 各跑 5 次取最小
    j7 = ''
    if a.old_cc:
        o = a.old_cc if os.path.isabs(a.old_cc) else os.path.join(os.path.dirname(HERE), a.old_cc)
        if not os.path.exists(o):
            bad.append('J7: 旧编译器不存在 %s' % o)
        else:
            tn = time_exe(cc, work, 'new')
            to = time_exe(o, work, 'old')
            if tn and to:
                ratio = tn[0] / to[0] if to[0] > 0 else 0.0
                j7 = ('J7 运行性能: 新 %.1f ms / 旧 %.1f ms（5 跑取最小）= %.2fx（上限 1.15x）'
                      % (tn[0] * 1000, to[0] * 1000, ratio))
                print(j7)
                if ratio > 1.15:
                    bad.append('J7: 运行时间退化 %.2fx > 1.15x' % ratio)
            else:
                bad.append('J7: 运行探针 编/跑 失败')
    else:
        print('J7 运行性能: **未测**（未给 --old-cc）—— 这不是通过，是缺测；本仓证据跑须带 --old-cc')

    if bad:
        print('\n[失败 FAIL] ' + '；'.join(bad))
        return 1
    print('\n[通过 PASS] switch 已是线性/跳转表：N=80 jmp=%d，每翻倍增长 %.2f/%.2f'
          % (m80['jmp'], r1, r2))
    if j7:
        print('           ' + j7)
    return 0


if __name__ == '__main__':
    sys.exit(main())
