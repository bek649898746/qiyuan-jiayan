#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_gate_d32 —— 全量验收闸 **零锚假绿** 判据（审计递刀 D32 · 2026-10-18）。

病灶：判据曾是
    `fail_l or (gate and skip_l) or gate_fails`
⟹ 一个锚都没跑（pass_n == 0，其余皆空）时整条为 False ⟹ 打印「==== 全绿 ====」并 exit 0。
锚清单被清空 / 名字写错 / CORE_FEAT 损坏 都能骗过闸 —— **量具错了，后面所有「全绿」都不作数**。

本判据直接调用**随脚本发布的**判决函数 `run_tests_zh.闸判`（不是复制一份逻辑），
对 7 组输入断言退出码，其中第 1 组就是 D32 的假绿输入。

用法: python scripts/check_gate_d32.py
退出码: 0 = 全过；1 = 有 RED
"""
import os, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'scripts'))
import run_tests_zh as G  # noqa: E402

# (pass_n, skip_l, fail_l, gate_fails, gate, 期望退出码, 说明)
CASES = [
    (0, [], [], [], True, 1, u'D32 零锚执行（假绿）⟹ 必须拒绝'),
    (30, [], [], [], True, 0, u'30 锚全过 ⟹ 绿'),
    (30, [('a', u'跳过')], [], [], True, 1, u'gate 模式出现 SKIP ⟹ 红'),
    (0, [], [('a', u'失败')], [], True, 1, u'有 FAIL ⟹ 红'),
    (0, [], [], [u'闸检查未过'], True, 1, u'gate_fails 非空 ⟹ 红'),
    (1, [], [], [], False, 0, u'--only/--zh-feat-all 模式（非闸）⟹ 单测绿可 exit 0'),
    (0, [('a', u'跳过')], [], [], False, 0, u'非闸模式 允许 SKIP'),
]


def main():
    fails = []
    for pass_n, skip_l, fail_l, gf, gate, want, desc in CASES:
        rc, gf2 = G.闸判(pass_n, skip_l, fail_l, gf, gate)
        ok = (rc == want) and (want != 1 or len(gf2) >= len(gf))
        print('[%s] pass=%d skip=%d fail=%d gate=%s -> rc=%d (期望 %d)  %s'
              % ('PASS' if ok else 'FAIL', pass_n, len(skip_l), len(fail_l), gate, rc, want, desc))
        if not ok:
            fails.append(desc)
    # 旧逻辑对照：证明这不是「本来就会红」
    old = bool([] or (True and []) or [])
    print('[对照] 旧判据对 (0,[],[],[],True) 的求值 = %s ⟹ 旧版会 exit 0（假绿）' % old)
    if old is not False:
        fails.append('旧逻辑对照异常（预期 False）')
    print()
    if fails:
        print('=== gate-d32: RED (%d 项) ===' % len(fails))
        for f in fails: print('  - ' + f)
        return 1
    print('=== gate-d32: PASS ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
