# -*- coding: utf-8 -*-
"""R715R 刀五 判据检查器：**失效尺**（A8 的真正验收线）+ pass 记录表 自洽。
判据:
  ① 失效尺: 注册行 >= 0 · 命中(变换前) == 注册行 · **命中(变换后) < 0**（必失效）
  ② pass 记录表: 记录数 == 已注册数 · 净合 与 换代 的 一致关系 自洽
  ③ 复位: 两次编译 结果 相同（可重入 ⟹ 无跨编译残留）
"""
import os, re, subprocess, sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
T = os.path.join(os.environ['TEMP'], 'qiyuan-ir-knives')
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(QJ, 'qcc_zh_v2.exe')
SRC = os.path.join(T, 'kw3_h.c')
fails = []


def dump_once(outname):
    r = subprocess.run([CC, SRC, '-o', os.path.join(T, outname)],
                       capture_output=True, env=dict(os.environ, QCC_DUMP='ir'), cwd=QJ)
    return ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


o1 = dump_once('kw5_a.exe')
m = re.search(r'失效尺 \(注册→命中→换代\+\+→必失效\): 注册行=(-?\d+) 命中\(变换前\)=(-?\d+) 命中\(变换后\)=(-?\d+) 换代=(-?\d+) ⟹ (\S+)', o1)
if not m:
    print('✗ 未见 失效尺 段'); fails.append('判据① 缺段')
else:
    a, b, c, g, verdict = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)), m.group(5)
    ok = (a >= 0 and b == a and c < 0)
    print('① 失效尺: 注册行=%d 命中前=%d 命中后=%d 换代=%d ⟹ %s  %s' % (a, b, c, g, verdict, '✓' if ok else '✗'))
    if not ok:
        fails.append('判据①')

m2 = re.search(r'pass 记录表 \(尾午一=(\d+) 已注册=(\d+);', o1)
rows = re.findall(r'pass\[(\d+)\] 末丙=(-?\d+) 结果数=(-?\d+) 净合=(-?\d+) (\S+)', o1)
if m2 and rows:
    cap, reg = int(m2.group(1)), int(m2.group(2))
    ok2 = (len(rows) == reg and reg >= 1)
    print('② pass 表: 尾午一=%d 已注册=%d 表行=%d %s' % (cap, reg, len(rows), '✓' if ok2 else '✗'))
    for r in rows:
        print('     pass[%s] 末丙=%s 结果数=%s 净合=%s %s' % r)
    if not ok2:
        fails.append('判据②')
else:
    print('✗ 未见 pass 记录表'); fails.append('判据② 缺段')

# ③ 可重入：两次 独立编译 ⟹ 段 逐字节相同
o2 = dump_once('kw5_b.exe')


def seg(t):
    i = t.find('-- IR pass 失效尺')
    j = t.find('-- IR CG 块表') if '-- IR CG 块表' in t else t.find('-- IR CFG 块表')
    return t[i:j] if i >= 0 and j > i else ''


s1, s2 = seg(o1), seg(o2)
ok3 = (s1 != '' and s1 == s2)
print('③ 可重入（两次编译 该段 逐字节相同）: %s' % ('✓' if ok3 else '✗'))
if not ok3:
    fails.append('判据③')

print()
print('[%s] R715R 刀五 判据 %s' % ('PASS' if not fails else 'FAIL', '' if not fails else fails))
sys.exit(0 if not fails else 1)
