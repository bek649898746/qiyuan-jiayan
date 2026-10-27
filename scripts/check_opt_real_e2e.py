#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_opt_real_e2e —— 真实 `cc -S` 产物的端到端判据（goal plan Verification step 3）。

取材：`run_tests_zh.py` 的 `CORE_FEAT` 里按序取一个**能编能跑**的锚（默认 scripts/feat_zh_2phase30_ok.c），
走 shipped 编译器产出 `-S` 中间态（含 `; @QCCMETA` 附录），然后：

  ① 优化器必须打印「元数据 … 良构」「结构 块=… 边=… 前驱表 良构 活集 迭代=…」；
  ② 结构级优化在真实产物上**有命中**（LSP 槽 / 跨块转发 / 压栈回读 三者至少一个 > 0）；
  ③ 优化链 `.text` 有效内容差 **== 汇编器自报 总缩**（自报与实测不得脱钩）；
  ④ 两链（未优化 / 优化）运行 **退出码与 stdout 逐字节相同**。

用法: python scripts/check_opt_real_e2e.py [--cc qcc_zh_v1.exe] [--src scripts/feat_zh_2phase30_ok.c]
退出码: 0 = 全过 / 1 = 有红 / 2 = 环境未就绪
"""
import argparse
import io
import os
import re
import struct
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)


def run(cmd, t=900):
    return subprocess.run(cmd, capture_output=True, timeout=t)


def text_size(p):
    d = io.open(p, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    o = struct.unpack_from('<H', d, pe + 20)[0]
    for i in range(n):
        q = pe + 24 + o + 40 * i
        if d[q:q + 8].rstrip(b'\x00').decode('latin1') == '.text':
            vsz, va, rsz, ro = struct.unpack_from('<IIII', d, q + 8)
            b = d[ro:ro + rsz]
            j = len(b) - 1
            while j >= 0 and b[j] == 0:
                j -= 1
            return j + 1
    return -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--src', default='scripts/feat_zh_2phase30_ok.c')
    ap.add_argument('--work', default=os.path.join(os.environ.get('TEMP', '.'), 'qcc_real_e2e'))
    a = ap.parse_args()
    for p in (a.cc, 'opt_zh.exe', 'asm_zh.exe', a.src):
        if not os.path.exists(p):
            print('[FAIL] 环境未就绪（缺 %s）' % p)
            return 2
    os.makedirs(a.work, exist_ok=True)
    base = os.path.join(a.work, 'e2e')

    print('== 真实 -S 产物端到端（cc=%s · src=%s）==' % (a.cc, a.src))
    r = run([a.cc, '-S', a.src, '-o', base + '.exe'])
    if r.returncode != 0:
        print('✗ 未优化链 编译失败 rc=%d\n%s' % (r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')[:400]))
        return 1
    src_asm = base + '.exe.asm'
    txt = io.open(src_asm, encoding='utf-8', errors='replace').read()
    print('  中间态 %s（%d 行 · 附录=%s）' % (src_asm, txt.count('\n'), '@QCCMETA' in txt))
    if '@QCCMETA' not in txt:
        print('✗ 中间态缺元数据附录（结构层无法启用）')
        return 1

    opt_out = base + '_o.asm'
    ro = run(['opt_zh.exe', src_asm, '-o', opt_out])
    o = (ro.stdout + ro.stderr).decode('utf-8', 'replace')
    if ro.returncode != 0:
        print('✗ 优化器 rc=%d\n%s' % (ro.returncode, o[:400]))
        return 1
    ok_meta = ('良构' in o) and ('覆盖率' in o)
    mst = re.search(r'\[结构\] 块=(\d+) 边=(\d+) .*活集 迭代=(\d+)', o)
    print('  ① 元数据/结构: %s %s' % ('良构' if ok_meta else '★ 缺良构行',
                                     ('块=%s 边=%s 活集迭代=%s' % mst.groups()) if mst else ''))

    def g(pat):
        m = re.search(pat, o)
        return int(m.group(1)) if m else 0
    lsp = g(r'LSP 槽=(\d+)')
    xblk = g(r'存→取 转发 条=(\d+)')
    fold = g(r'压栈回读 (\d+)')
    dce = g(r'G-DCE (\d+)')
    print('  ② 真实产物命中: LSP 槽=%d · 跨块转发=%d · 压栈回读=%d · G-DCE=%d' % (lsp, xblk, fold, dce))
    ok_hit = (lsp + xblk + fold) > 0

    ra = run(['asm_zh.exe', src_asm, '-o', base + '_n.exe'])
    rb = run(['asm_zh.exe', opt_out, '-o', base + '_o.exe'])
    if ra.returncode or rb.returncode:
        print('✗ 汇编失败 rc=%d/%d' % (ra.returncode, rb.returncode))
        return 1
    ta = (ra.stdout + ra.stderr).decode('utf-8', 'replace')
    tb = (rb.stdout + rb.stderr).decode('utf-8', 'replace')
    mz = re.search(r'总缩=(-?\d+)', tb)
    total = int(mz.group(1)) if mz else None
    dn, do = text_size(base + '_n.exe'), text_size(base + '_o.exe')
    delta = dn - do
    print('  ③ .text %d → %d（差 %d）· 汇编器自报 总缩=%s ⟹ %s'
          % (dn, do, delta, total, '一致' if total == delta else '★漂移'))

    p1 = subprocess.run([base + '_n.exe'], capture_output=True, timeout=300)
    p2 = subprocess.run([base + '_o.exe'], capture_output=True, timeout=300)
    same = (p1.returncode == p2.returncode) and (p1.stdout == p2.stdout)
    print('  ④ 两链运行: rc %d/%d · stdout 相同=%s' % (p1.returncode, p2.returncode, p1.stdout == p2.stdout))

    # ⑤ 写者保全闸（R881L 的核心不变式）—— **差分口径**（整文件里别的 pass 也会删 `压栈`，
    #    例如 ⑪ ⑫ 的平衡 push/pop 融合，所以不能直接数条数）：
    #    把 ⑮ 关掉（源码里 `&& 压栈回读(k) != 0` → `&& 0`）再跑一遍，
    #    两边的 `.删 …压栈` 条数必须**完全相同** ⟹ ⑮ 没删过任何写者。
    nofold_src = os.path.join(a.work, 'opt_nofold.jy')
    t = io.open('srclib/opt_zh.jy', encoding='utf-8').read()
    t2 = t.replace('&& 压栈回读(k) != 0)', '&& 0)', 1)
    if t2 == t:
        print('  [FAIL] 关不掉 ⑮（找不到调用点锚）')
        return 1
    io.open(nofold_src, 'w', encoding='utf-8', newline='\n').write(t2)
    nofold_exe = os.path.join(a.work, 'opt_nofold.exe')
    rb0 = run([a.cc, nofold_src, '-o', nofold_exe])
    if rb0.returncode != 0:
        print('  [FAIL] 无 ⑮ 的对照优化器 编译失败')
        return 1
    nofold_out = base + '_nofold.asm'
    rb1 = run([nofold_exe, src_asm, '-o', nofold_out])
    if rb1.returncode != 0:
        print('  [FAIL] 对照优化器 运行失败')
        return 1
    otxt = [l for l in io.open(opt_out, encoding='utf-8', errors='replace').read().split('\n')
            if l.startswith('.删') and '压栈' in l]
    ntxt = [l for l in io.open(nofold_out, encoding='utf-8', errors='replace').read().split('\n')
            if l.startswith('.删') and '压栈' in l]
    fold_no = re.search(r'压栈回读 (\d+)', (rb1.stdout + rb1.stderr).decode('utf-8', 'replace'))
    f0 = int(fold_no.group(1)) if fold_no else 0
    print('  ⑤ 写者保全（差分）：`.删 …压栈` 全开=%d 条 · 关掉 ⑮=%d 条（此时 ⑮=%d）⟹ %s'
          % (len(otxt), len(ntxt), f0, '⑮ 没删任何写者 ✓' if len(otxt) == len(ntxt) else '★⑮ 删了写者'))

    bad = []
    if not ok_meta:
        bad.append('缺元数据/结构良构行')
    if not ok_hit:
        bad.append('真实产物上结构级优化 0 命中')
    if total is None or total != delta:
        bad.append('自报总缩 %s ≠ .text 差 %d' % (total, delta))
    if not same:
        bad.append('两链 退出码/stdout 不同')
    if len(otxt) != len(ntxt):
        bad.append('⑮ 删了 %d 条 `压栈`（写者必须保留）' % (len(otxt) - len(ntxt)))
    print('=' * 60)
    print('真实产物端到端: %s' % ('PASS' if not bad else 'FAIL: ' + '；'.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
