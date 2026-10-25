# -*- coding: utf-8 -*-
"""check_ir_readend —— **IR 侧表读端真接入** 判据（plan 验收判据 1 / R821）。

它证什么（不是「有计数」就算）：
  ① `段双[]`（IR 节点级 截断标记，唯一源 = `建节()` 对 窄转型节点 打 2）在 码生成期 被**真读**：
     外层 窄转型 的 操作数 本身 是 **同宽同符号** 的 窄转型 ⟹ 掩码+扩展 冗余 ⟹ 省掉；
  ② 读端计数 `读段双` 出现在 IR dump 且 > 0（命中数 = 读端驱动变换的次数）；
  ③ 去掉该读端（用父编译器 = 本改动前的不动点）时，该变换**消失** ⟹ 1063（抽截断）计数差回到 3。

判据用**差分**（单层探针 vs 嵌套探针），不写死任何绝对计数 —— 前插运行库自身的截断发射
在两次编译里完全相同，差分只反映被测形态。

用法:
  python scripts/check_ir_readend.py [--cc qcc_zh_seed.exe]
  python scripts/check_ir_readend.py --parent <父编译器.exe>    # 负控：必须 差==3 且 无读端
退出码: 0 = 读端真接入（或负控按预期红）；1 = 不成立
"""
import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE_S = os.path.join(ROOT, 'scripts', 'probe', 'ir_readend_single.c')
PROBE_D = os.path.join(ROOT, 'scripts', 'probe', 'ir_readend_double.c')
EXPECT = 'R821 44 4464 212'


def compile_and_run(cc, src):
    w = tempfile.mkdtemp(prefix='ir_readend_')
    exe = os.path.join(w, 'p.exe')
    env = dict(os.environ, QCC_DUMP='ir')
    r = subprocess.run([cc, src, '-o', exe], capture_output=True, env=env, timeout=600)
    out = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    if r.returncode != 0:
        return None, None, out
    q = subprocess.run([exe], capture_output=True, timeout=120)
    prog = (q.stdout or b'').decode('utf-8', 'replace').strip()
    return out, prog, out


def counts(dump):
    c1063 = len(re.findall(r'算子=1063', dump))
    m = re.search(r'读段双=(\d+)', dump)
    return c1063, (int(m.group(1)) if m else None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    ap.add_argument('--parent', default='', help='负控：本改动前的编译器（必须复现旧行为）')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 2

    d1, o1, raw1 = compile_and_run(cc, PROBE_S)
    d2, o2, raw2 = compile_and_run(cc, PROBE_D)
    if d1 is None or d2 is None:
        print('[FAIL] 探针编译失败')
        print(raw1[-400:] if d1 is None else raw2[-400:])
        return 1
    c1, r1 = counts(d1)
    c2, r2 = counts(d2)
    print('  单层探针: 1063=%d 读段双=%s 运行=%r' % (c1, r1, o1))
    print('  嵌套探针: 1063=%d 读段双=%s 运行=%r' % (c2, r2, o2))
    fails = []
    if o1 != EXPECT or o2 != EXPECT:
        fails.append('运行输出不符: %r / %r 应 %r' % (o1, o2, EXPECT))
    delta = c2 - c1
    if a.parent:
        pc = a.parent if os.path.isabs(a.parent) else os.path.join(ROOT, a.parent)
        if not os.path.exists(pc):
            print('[FAIL] 父编译器不存在: %s' % pc)
            return 2
        p1, po1, _ = compile_and_run(pc, PROBE_S)
        p2, po2, _ = compile_and_run(pc, PROBE_D)
        if p1 is None or p2 is None:
            print('[FAIL] 父编译器 探针编译失败')
            return 1
        q1, qr1 = counts(p1)
        q2, qr2 = counts(p2)
        print('  父控 单层: 1063=%d 读段双=%s 运行=%r' % (q1, qr1, po1))
        print('  父控 嵌套: 1063=%d 读段双=%s 运行=%r' % (q2, qr2, po2))
        # 负控预期：读端不存在 ⟹ 嵌套多 3 条掩码；运行结果仍正确（旧行为）
        ok = (q2 - q1 == 3) and (qr1 is None) and (qr2 is None) and po1 == po2 == EXPECT
        print('  负控判定: 差=%d (应 3) · 读段双=%s/%s (应 无) · 运行=%r/%r (应 %r)'
              % (q2 - q1, qr1, qr2, po1, po2, EXPECT))
        print('=== IR 读端负控: %s ===' % ('PASS（旧行为复现，判据可变红）' if ok else 'FAIL'))
        return 0 if ok else 1

    if r2 is None or r2 <= 0:
        fails.append('读段双 计数缺失或为 0（读端未真读；嵌套探针 应 > 0）')
    if delta != 0:
        fails.append('嵌套探针 1063 差 = %d（应 0 ⟹ 外层掩码被读端省掉）' % delta)
    print('  差分: 嵌套 - 单层 = %d（应 0）' % delta)
    if fails:
        print('[FAIL] IR 读端 判据不成立:')
        for f in fails:
            print('   -', f)
        return 1
    print('[PASS] IR 读端真接入：段双 被读（嵌套探针 读段双=%d）⇹ 冗余掩码省掉（1063 差=0），运行结果不变' % r2)
    return 0


if __name__ == '__main__':
    sys.exit(main())
