#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_corpus_lua —— D6 真实语料 lua 5.4.7 编译档判据（核心库 33/33）。

修前实证（2026-11-03 · 虾米）：
  `ldo.c` `finishCcall` 的 `(*ci->u.c.k)(L, status, ci->u.c.ctx)` 报 A9010 ——
  `CallInfo.u.c.k` 是 `lua_KFunction`（typedef 函数指），嵌在「union → 匿名 struct」
  两层内联体里；该字段的 `字段类型序` 未登记为 -2（-1），`表达式层级` 于是判
  「明确非函数指」。

  两处根治：
  1. 内联体（`内联二` 三处）的 typedef 分支补 `是否函数指` → 尺寸 8 + `符聚`；
     `字段登记` 增加 `字段函指待` 单点消费（写 `毛利 = -2`）。
  2. `表达式层级` 成员链：字段类型序 == -1 但字段是 **8 字节非指针** 且未登记
     函数指星数时，按层级 0 处理（8B 值本身即函数指针，C 6.3.2.1），不多解引用。

断言：
  1. 核心库 33 个 .c（排除 `onelua.c` 聚合文件与 `luac.c` 工具）逐文件 `-I src -c` 全绿；
  2. `ldo.c` 在名单内且 rc=0（修前红，防回归）；
  3. `luac.c` 是**既有**工具缺陷（A9001 `Proto.p`，旧编译器同样报），显式挂账：
     若它变绿，本判据会打印「可更新名单」并判红，强制诚实收口。

用法: python scripts/check_corpus_lua.py [--cc qcc_zh_seed.exe] [--lua <src dir>]
退出码: 0 = PASS / 1 = RED / 2 = 语料未就绪（SKIP，显式打印）
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'scripts'))
from qcc_tmp import tmp_dir  # noqa: E402

DEFAULT_LUA = os.environ.get('QCC_LUA_DIR', r'D:\qcc_corpus\lua-5.4.7\src')
# 聚合文件 onelua.c 会把全部 .c 再包一层（甲言合并 TU 语义不同），单列。
SKIP = {'onelua.c'}
# 既有工具缺陷（非本轮修复面）；变绿时强制更新名单。
KNOWN_RED = {'luac.c': 'A9001'}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    ap.add_argument('--lua', default=DEFAULT_LUA)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[RED] 编译器不存在: %s' % cc)
        return 1
    if not os.path.isdir(a.lua):
        print('[SKIP] lua 语料未就绪: %s（设 QCC_LUA_DIR 或 --lua）' % a.lua)
        return 2

    w = tmp_dir('qcc_lua_compile')
    srcs = sorted(f for f in os.listdir(a.lua) if f.endswith('.c') and f not in SKIP)
    bad = []
    ok = 0
    for src in srcs:
        r = subprocess.run([cc, '-I', a.lua, '-c', src, '-o', os.path.join(w, src + '.o')],
                           cwd=a.lua, capture_output=True, text=True,
                           encoding='utf-8', errors='replace')
        out = (r.stdout or '') + (r.stderr or '')
        if src in KNOWN_RED:
            want = KNOWN_RED[src]
            if r.returncode == 0:
                bad.append('%s 已知红变绿（%s）⟹ 请更新 KNOWN_RED 名单' % (src, want))
            elif want not in out:
                bad.append('%s 失败但诊断不含 %s（可能换了病灶）: %s'
                           % (src, want, out.strip().replace('\n', ' ')[:160]))
            else:
                print('INFO %-14s 既有工具缺陷（%s）：%s' % ('known-red', want, src))
            continue
        if r.returncode != 0:
            bad.append('%s rc=%d %s' % (src, r.returncode, out.strip().replace('\n', ' ')[:160]))
        else:
            ok += 1

    core = [s for s in srcs if s not in KNOWN_RED]
    print('lua 5.4.7 核心库编译: %d/%d（跳过 %s；已知红 %s）'
          % (ok, len(core), ','.join(sorted(SKIP)), ','.join(sorted(KNOWN_RED))))
    if bad:
        print('\n'.join('[RED] ' + x for x in bad))
        print('== lua 编译档判据: RED（%d 项）==' % len(bad))
        return 1
    print('== lua 编译档判据: PASS（核心 33/33 + 1 条既有工具挂账）==')
    return 0


if __name__ == '__main__':
    sys.exit(main())
