#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_opt_perf —— 元数据驱动优化链 的 收益判据（R880）。

口径（可复算）：
  · 固定基准 1: `fib(36)` 递归
  · 固定基准 2: 双层循环（规模 取 使 **计算时间** 占主导 —— 本机 空程序 启动 ~29ms 是
    加载器/镜像的固定成本，两条链 同付，故 基准 必须 让 计算 压过 常数 才有 判别力）
  · 两条链: `cc -S` → `asm_zh`        vs   `cc -S` → `opt_zh` → `asm_zh`
  · 各跑 ≥5 次 取 **中位墙钟**；三条链：未优化 / **对照优化器**(R880) / 本次优化器
  · 判据（R881 终版，两条都要过）:
      `loop2`  : 本次 **≥33%**（R880 基线 23.4；R881 循环槽提升把内层归纳变量提到寄存器 ⟹ 36–38%）
      `fib36`  : 本次 **≥23%**（计划原文的绝对门槛）+ **不低于 同机同次 的 对照(R880) 基线**（容差 1.0pp）
    说明: 「23.0% 起点基线」是上一轮在别的时间点测的（同机复测 R880 只有 21.6–22.6%），
    所以两道一起判：绝对值达到 23%（R881 的压栈回读折叠把它抬到 ~26.8%），且不许相对基线倒退。
  · 三条链 程序输出必须 **逐字节相同**

用法: python scripts/check_opt_perf.py [--cc qcc_zh_v1.exe] [--runs 7] [--work <dir>]
退出码: 0 = 达标；1 = 未达标（如实报数）；2 = 环境未就绪
"""
import argparse
import io
import os
import statistics
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
CTL_SHA = 'a9f6b88a'   # R880d：本轮改动前的出厂优化器（同机 A/B 的基线）

FIB = '''#include <stdio.h>
int fib(int n){ if (n < 2) return n; return fib(n-1) + fib(n-2); }
int main(void){ printf("FIB %d\\n", fib(36)); return 0; }
'''
LOOP = '''#include <stdio.h>
int main(void){
    int s = 0;
    for (int i = 0; i < 9000; i = i + 1)
        for (int j = 0; j < 9000; j = j + 1)
            s = s + i * j;
    printf("LOOP %d\\n", s);
    return 0;
}
'''


def run(cmd, t=1800):
    return subprocess.run(cmd, capture_output=True, timeout=t)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--runs', type=int, default=9)
    ap.add_argument('--control', default=os.path.join('build', 'control_opt_R880.exe'),
                    help='对照优化器（R880 出厂版）；缺省则从 git 提交 %s 现编' % CTL_SHA)
    ap.add_argument('--work', default=os.path.join(os.environ.get('TEMP', '.'), 'qcc_opt_perf'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    opt = os.path.join(ROOT, 'opt_zh.exe')
    if not os.path.exists(opt):
        r = subprocess.run([cc, os.path.join(ROOT, 'srclib', 'opt_zh.jy'), '-o', opt], capture_output=True)
        if r.returncode != 0:
            print('[FAIL] 出厂优化器 缺失 且 现编 失败'); return 2
    asm = os.path.join(ROOT, 'asm_zh.exe')
    ctl = a.control if os.path.isabs(a.control) else os.path.join(ROOT, a.control)
    if not all(os.path.exists(x) for x in (cc, opt, asm)):
        print('[FAIL] 环境未就绪: cc/opt/asm 缺一'); return 2
    os.makedirs(a.work, exist_ok=True)
    if not os.path.exists(ctl):
        # `build/` 下 .exe 不入库 ⟹ 从固定提交 现编 基线件（可复算，不依赖手工拷贝）
        r = subprocess.run(['git', 'show', '%s:srclib/opt_zh.jy' % CTL_SHA], capture_output=True)
        if r.returncode != 0:
            print('[FAIL] 取不到基线源 git show %s:srclib/opt_zh.jy —— 同机 A/B 需要它（fail-closed）' % CTL_SHA)
            return 2
        base_src = os.path.join(a.work, 'opt_zh_%s.jy' % CTL_SHA)
        io.open(base_src, 'wb').write(r.stdout)
        ctl = os.path.join(a.work, 'control_opt_%s.exe' % CTL_SHA)
        rb = subprocess.run([cc, base_src, '-o', ctl], capture_output=True)
        if rb.returncode != 0 or not os.path.exists(ctl):
            print('[FAIL] 基线优化器 现编失败 rc=%d' % rb.returncode)
            return 2
        print('[基线] 由 git %s:srclib/opt_zh.jy 现编 ⟹ %s' % (CTL_SHA, os.path.basename(ctl)))
    os.makedirs(a.work, exist_ok=True)
    fails = []

    def build(name, src):
        p = os.path.join(a.work, name + '.c')
        io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
        base = os.path.join(a.work, name)
        r = run([cc, '-S', p, '-o', base + '_n.exe'])
        if r.returncode != 0:
            raise SystemExit('%s -S 失败 rc=%d' % (name, r.returncode))
        s = base + '_n.exe.asm'
        r = run([asm, s, '-o', base + '_n_a.exe'])
        if r.returncode != 0:
            raise SystemExit('%s asm(未优化) 失败' % name)
        for tag, o in (('_o', opt), ('_c', ctl)):
            r = run([o, s, '-o', base + tag + '.asm'])
            if r.returncode != 0:
                raise SystemExit('%s opt(%s) 失败: %s'
                                 % (name, tag, (r.stdout + r.stderr).decode('utf-8', 'replace')[:200]))
            r = run([asm, base + tag + '.asm', '-o', base + tag + '_a.exe'])
            if r.returncode != 0:
                raise SystemExit('%s asm(%s) 失败' % (name, tag))
        return base

    def bench(exe):
        ts, out = [], ''
        for _ in range(a.runs):
            t0 = time.perf_counter()
            p = run([exe], 900)
            ts.append(time.perf_counter() - t0)
            out = (p.stdout or b'').decode('utf-8', 'replace')
        return statistics.median(ts), out

    print('== 优化链 收益 (cc=%s, 对照=%s, runs=%d) =='
          % (os.path.basename(cc), os.path.basename(ctl), a.runs))
    FIB_TOL = 1.0     # 同机同次的实测噪声底（pp）
    for name, src in (('fib36', FIB), ('loop2', LOOP)):
        base = build(name, src)
        # 三条链 交替跑（同机同次 ⟹ 对照与本次 共享同一环境噪声）
        ts = {k: [] for k in ('n', 'c', 'o')}
        outs = {}
        for _ in range(a.runs):
            for k, exe in (('n', base + '_n_a.exe'), ('c', base + '_c_a.exe'), ('o', base + '_o_a.exe')):
                t0 = time.perf_counter()
                p = run([exe], 900)
                ts[k].append(time.perf_counter() - t0)
                outs[k] = (p.stdout or b'').decode('utf-8', 'replace')
        mn, mc, mo = (statistics.median(ts[k]) for k in ('n', 'c', 'o'))
        gn = (mn - mc) / mn * 100.0
        go = (mn - mo) / mn * 100.0
        same = (outs['n'] == outs['c'] == outs['o'])
        print('%-6s 未优化 %.4fs | 对照(R880) %.4fs 提速 %5.1f%% | 本次 %.4fs 提速 %5.1f%% | 输出 %s'
              % (name, mn, mc, gn, mo, go, 'SAME' if same else 'DIFF'))
        if not same:
            fails.append('%s 三条链 输出 不一致' % name)
        if name == 'fib36':
            if go < 23.0:
                fails.append('fib36 本次 %.1f%% < 23%%（计划绝对门槛）' % go)
            if go < gn - FIB_TOL:
                fails.append('fib36 本次 %.1f%% 低于同机基线 %.1f%% - %.1fpp' % (go, gn, FIB_TOL))
        else:
            if go < 33.0:
                fails.append('loop2 本次 %.1f%% < 33%%' % go)
            if go < gn - FIB_TOL:
                fails.append('loop2 本次 %.1f%% 低于同机基线 %.1f%%' % (go, gn))

    print('=' * 60)
    if fails:
        print('优化 收益 判据: FAIL -> %s' % ', '.join(fails))
        return 1
    print('优化 收益 判据: PASS（loop2 ≥33% · fib36 ≥23% 且不低于同机 R880 基线 · 三条链 输出 逐字节 相同）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
