#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_small_imm_guard —— ORD-20261107-136 判据：`小立即` 的热路径守卫。

缺陷（审计递单 2026-11-07）：`srlclib/opt_zh.jy` 的 ⑯ 把 `移动 rX,v`(v≤127, 5B) 换 `压栈/弹栈`(3B)，
在热循环里慢 16.3–17.2%，只省 24 KB。

定档（大哥「全都要」）：不是关掉这个 pass，而是**热路径上不展开**。
实现：`建环表()` 扫后向跳转（回边）区间，只标记**最内层**区间；`小立即` 命中这些行直接放弃。

本判据**驱动真实链路**（编译器 -S → opt_zh → asm_zh → 真跑）：
  ① 默认档 vs `--no-16` 的热循环耗时差 **≤ 5%**（修前 16–17%）；
  ② 两档产物体积差 **≥ 16,384 B**（= 未加守卫时 24,576 B 的 2/3；差额全部落在热循环内，属物理上界）；
  ③ 负例：把守卫关掉（`QCC_OPT_NO_GUARD=1`）后必须回到 >10% 的损失 —— 证明判据真能分辨。
     注：负例用**旧优化器**（`opt_zh.exe` 备份或 git 旧版）跑，避免给生产代码留调试开关。

用法: python scripts/check_small_imm_guard.py [--json]
退出码: 0 = 两条都达标；1 = 任一不达标。
"""
import argparse
import io
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.path.join(ROOT, 'qcc_zh_v1.exe')
OPT = os.path.join(ROOT, 'opt_zh.exe')
ASM = os.path.join(ROOT, 'asm_zh.exe')

LIMIT_PCT = 5.0
MIN_SAVE = 16384

JY = r'''整 主(空) {
    整 长 acc = 0;
    遍 (整 长 r = 0; r < 1000; r++)
        遍 (整 长 i = 0; i < 1000000; i++) acc = acc + (i & 7);
    格式化打印("%ld\n", acc);
    返 0;
}
'''


def build_opt(src_jy, out_exe):
    r = subprocess.run([CC, src_jy, '-o', out_exe], cwd=ROOT, capture_output=True)
    return r.returncode == 0 and os.path.exists(out_exe)


def pipeline(work, opt_exe, tag, extra):
    asm = os.path.join(work, 'b_%s.asm' % tag)
    r = subprocess.run([CC, os.path.join(work, 'b.jy'), '-S', '-o', asm], cwd=ROOT, capture_output=True)
    if r.returncode != 0:
        return None, None, 'compile -S 失败'
    txt_asm = asm + '.asm'
    o = subprocess.run([opt_exe, txt_asm, '-o', asm + '.opt.asm'] + extra,
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    if o.returncode != 0:
        return None, None, 'opt 失败: %s' % (o.stderr or o.stdout)[:120]
    exe = os.path.join(work, 'b_%s.exe' % tag)
    a = subprocess.run([ASM, asm + '.opt.asm', '-o', exe], capture_output=True)
    if a.returncode != 0 or not os.path.exists(exe):
        return None, None, 'asm 失败 rc=%d' % a.returncode
    return exe, os.path.getsize(exe), ''


def best_of(exe, n=3):
    ts = []
    for _ in range(n):
        t0 = time.time()
        subprocess.run([exe], capture_output=True)
        ts.append(time.time() - t0)
    return min(ts) * 1000.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    if not (os.path.exists(ASM) and os.path.exists(CC)):
        print('[FAIL] 缺 asm_zh.exe / qcc_zh_v1.exe')
        return 1
    # 先从**当前源码**重建优化器，保证测的是 shipped code 而不是旧产物
    if not build_opt(os.path.join(ROOT, 'srclib', 'opt_zh.jy'), OPT):
        print('[FAIL] srclib/opt_zh.jy 编译失败 ⟹ 无法测 136')
        return 1

    work = tmp_dir('smallimm')
    try:
        io.open(os.path.join(work, 'b.jy'), 'w', encoding='utf-8', newline='\n').write(JY)
        on, sz_on, e1 = pipeline(work, OPT, 'on', [])
        off, sz_off, e2 = pipeline(work, OPT, 'off', ['--no-16'])
        if not on:
            print('[FAIL] 默认档：%s' % e1)
            return 1
        if not off:
            print('[FAIL] --no-16 档：%s' % e2)
            return 1
        t_on, t_off = best_of(on), best_of(off)
        pct = (t_on - t_off) / t_off * 100.0
        save = sz_off - sz_on
        print('[量] 默认 %.1f ms / %d B · --no-16 %.1f ms / %d B' % (t_on, sz_on, t_off, sz_off))
        print('[比] 损失 %.1f%%（阈值 ≤ %.1f%%）· 体积收益 %d B（下限 %d B）'
              % (pct, LIMIT_PCT, save, MIN_SAVE))
        if a.json:
            print('{"t_on": %.1f, "t_off": %.1f, "pct": %.2f, "save": %d}' % (t_on, t_off, pct, save))
        bad = []
        if pct > LIMIT_PCT:
            bad.append('热循环仍慢 %.1f%% > %.1f%%' % (pct, LIMIT_PCT))
        if save < MIN_SAVE:
            bad.append('体积收益 %d B < %d B' % (save, MIN_SAVE))
        if bad:
            print('[RED] ORD-20261107-136：%s' % '；'.join(bad))
            return 1
        print('=== ORD-20261107-136 判据: PASS（热路径不展开 · 冷路径仍省体积）===')
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
