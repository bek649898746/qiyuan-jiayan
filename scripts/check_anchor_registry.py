#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_anchor_registry —— 锚登记表硬闸（2026-10-26 · 审计 D77/D78 递刀）。

治的是两类「数字看着对、其实没接上」的腐坏：
  · **口径三数**：盘上 `feat_zh_*.c` 文件数 ≠ `CORE_FEAT` 条目数 ≠ 文档里写的数
    （2026-10-26 审计回执 D77：README 5 处写 1,443、2 处写 1,656/1,663，各不相同）；
  · **悬挂/漏登**：清单引用已改名或删除的文件（D78），或新锚写进 `scripts/` 却从未入闸
    （实测 2026-10-26：19 个 `feat*.c` 不在任何闸内，其中 6 个一跑就绿 —— 纯漏登）。

六项机械检查（全过才 PASS）：
  (1) `CORE_FEAT` 无重复条目（重复 = 同一锚跑两遍，且掩盖重复登记）
  (2) `CORE_FEAT` 每条都在盘上（防 D78 类悬挂引用）
  (3) 盘上每个 `scripts/feat*.c` 要么在闸内，要么在 `EXCLUDED` 显式排除表（附理由）
  (4) `EXCLUDED` 表不腐坏：表里的锚不得同时在闸内、也不得已从盘上消失
  (5) `scripts/gate_baselines.json` 的 `gates.zh_feat.anchor_count` == `len(CORE_FEAT)`
      （该文件由 `memory/atlas/tools/gen_entry.py` 生成，`build_zh.ps1` 末尾自动重跑）
  (6) README 顶部 `GEN:STATUS` 块里的「特性锚」数字 == 现算值（由 `gen_readme_status.ps1` 生成）

用法:
  python scripts/check_anchor_registry.py            # 判定（CI/收口用）
  python scripts/check_anchor_registry.py --report   # 只打印口径分解
退出: 0 全过 / 1 有红
"""
import argparse
import collections
import glob
import io
import json
import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'scripts'))

# ── 显式排除表（每个都必须写理由；新增排除项要在这里加行，不许只在心里记）──────────
EXCLUDED = {
    # 依赖配套实现，单文件不链（文件头已标 @DEP / @MULTI）
    'feat_complex_zh.c': '@DEP 复数配套实现，单文件不链',
    'feat_map10_zh.c': '@DEP 映射库配套（文件 IO 词），单文件不链',
    'feat_lock.c': '@DEP v1 块尾解锁语义时代的锚，语言已升级为 defer 语义',
    'feat_zh_encap197_a.c': '@MULTI 跨 .o 链接对（a 文件，本文件无主）',
    'feat_zh_encap197_b.c': '@MULTI 跨 .o 链接对（b 文件，需与 a 链接）',
    # 长期红锚：入册规则要求「该锚 PASS 且全量闸 FAIL=0」，未达标前只留在盘上
    'feat_rawstr_zh.c': '长期红（2026-10-17 刀三移出 CORE_FEAT），修复后按入册规则回归',
    'feat_vartmpl.c': '长期红（2026-10-17 刀三移出 CORE_FEAT），修复后按入册规则回归',
    # 需要一个专用驱动器（@KNOB 环境开关），单跑必然「负例失效」
    'feat_zh_irland_l1cap_chain_neg.c': '由 scripts/check_ir_cap_neg.py 驱动（@KNOB QCC_DBG_IRCAP_CHAIN），不入 CORE_FEAT',
    'feat_zh_irland_l1cap_main_neg.c': '由 scripts/check_ir_cap_neg.py 驱动（@KNOB QCC_DBG_IRCAP_MAIN），不入 CORE_FEAT',
    # 调研/狗粮型探针，不是回归锚
    'feat_err2.c': '错误传播早期原型探针（非回归锚）',
    'feat_typedef.c': 'typedef 调研探针（非回归锚）',
    'feat_jyc_lib.c': '标准库吃狗粮程序（需链接 jylib，非单文件锚）',
    'feat_overload2.c': '运算符重载调研探针（C++ 语义模拟，非回归锚）',
}

STATUS_RE = re.compile(r'(?s)<!-- GEN:STATUS:BEGIN.*?GEN:STATUS:END -->')


def load_core_feat():
    import run_tests_zh as R
    return list(R.CORE_FEAT)


def registry():
    lst = load_core_feat()
    dup = sorted(k for k, v in collections.Counter(lst).items() if v > 1)
    missing = [x for x in lst if not os.path.exists(os.path.join(ROOT, x))]
    on_disk = sorted('scripts/' + os.path.basename(p)
                     for p in glob.glob(os.path.join(ROOT, 'scripts', 'feat*.c')))
    in_gate = set(lst)
    orphan = [p for p in on_disk if p not in in_gate]
    ungated = [p for p in orphan if os.path.basename(p) not in EXCLUDED]
    stale_excl = sorted(b for b in EXCLUDED
                        if 'scripts/' + b in in_gate
                        or not os.path.exists(os.path.join(ROOT, 'scripts', b)))
    zh_entries = [x for x in lst if os.path.basename(x).startswith('feat_zh_')]
    return dict(
        entries=lst, n_entries=len(lst), n_unique=len(set(lst)), dup=dup, missing=missing,
        on_disk=on_disk, n_disk=len(on_disk), orphan=orphan, ungated=ungated,
        stale_excl=stale_excl,
        n_zh_entries=len(zh_entries), n_other=len(lst) - len(zh_entries),
        n_zh_files=len([p for p in on_disk if os.path.basename(p).startswith('feat_zh_')]),
    )


def check_baselines(reg):
    p = os.path.join(ROOT, 'scripts', 'gate_baselines.json')
    if not os.path.exists(p):
        return ['gate_baselines.json 不存在（跑 gen_entry.py 生成）']
    try:
        d = json.load(io.open(p, encoding='utf-8'))
        got = d['gates']['zh_feat']['anchor_count']
    except Exception as e:
        return ['gate_baselines.json 读不出 zh_feat.anchor_count: %r' % (e,)]
    if got != reg['n_entries']:
        return ['gate_baselines zh_feat.anchor_count=%r ≠ len(CORE_FEAT)=%d ⟹ 重跑 gen_entry.py'
                % (got, reg['n_entries'])]
    return []


def check_readme(reg):
    p = os.path.join(ROOT, 'README.md')
    if not os.path.exists(p):
        return ['README.md 不存在']
    txt = io.open(p, encoding='utf-8', errors='replace').read()
    m = STATUS_RE.search(txt)
    if not m:
        return ['README 缺 GEN:STATUS 块（gen_readme_status.ps1 生成）']
    blk = m.group(0)
    row = [l for l in blk.splitlines() if '特性锚' in l]
    if not row:
        return ['README GEN:STATUS 缺「特性锚」行']
    line = row[0]
    bad = []
    if str(reg['n_entries']) not in line:
        bad.append('README 特性锚行缺条目数 %d: %s' % (reg['n_entries'], line.strip()[:110]))
    if str(reg['n_zh_files']) not in line:
        bad.append('README 特性锚行缺文件数 %d: %s' % (reg['n_zh_files'], line.strip()[:110]))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--report', action='store_true', help='只打印口径分解')
    ap.add_argument('--counts', action='store_true', help='机器可读：输出 entries=<n> files=<n>（供生成器调用）')
    a = ap.parse_args()
    reg = registry()

    if a.counts:
        print('entries=%d files=%d zh_entries=%d other=%d'
              % (reg['n_entries'], reg['n_zh_files'], reg['n_zh_entries'], reg['n_other']))
        return 0

    print('=== 锚口径分解（唯一口径 = run_tests_zh.py 的 CORE_FEAT）===')
    print('  CORE_FEAT 条目        %d（唯一 %d）' % (reg['n_entries'], reg['n_unique']))
    print('    其中 feat_zh_* 条目 %d' % reg['n_zh_entries'])
    print('    其中 其它条目       %d' % reg['n_other'])
    print('  盘上 feat*.c 文件     %d（其中 feat_zh_* %d）' % (reg['n_disk'], reg['n_zh_files']))
    print('  未入闸（排除表内）    %d' % (len(reg['orphan']) - len(reg['ungated'])))
    print('  未入闸（未登记）      %d %s' % (len(reg['ungated']), reg['ungated'][:6]))
    if a.report:
        return 0

    bad = []
    if reg['dup']:
        bad.append('(1) CORE_FEAT 有重复条目 %d: %s' % (len(reg['dup']), reg['dup']))
    if reg['missing']:
        bad.append('(2) CORE_FEAT 悬挂引用（文件不存在）%d: %s' % (len(reg['missing']), reg['missing'][:6]))
    if reg['ungated']:
        bad.append('(3) 有 %d 个锚既不在闸内、也不在 EXCLUDED 表: %s'
                   % (len(reg['ungated']), reg['ungated'][:6]))
    if reg['stale_excl']:
        bad.append('(4) EXCLUDED 表腐坏（已入闸或已从盘上消失）: %s' % reg['stale_excl'][:6])
    bad += ['(5) ' + s for s in check_baselines(reg)]
    bad += ['(6) ' + s for s in check_readme(reg)]

    if bad:
        print('\n=== 锚登记表闸: FAIL ===')
        for b in bad:
            print('  ✗ ' + b)
        print('  修法: 重复→删多余行; 悬挂→删条目或补文件; 漏登→加进 CORE_FEAT 或写进 EXCLUDED（附理由）;'
              ' 数字不符→重跑 gen_entry.py / gen_readme_status.ps1')
        return 1
    print('\n=== 锚登记表闸: PASS（条目 %d = feat_zh %d + 其它 %d；盘上文件 %d；排除 %d 有理由）==='
          % (reg['n_entries'], reg['n_zh_entries'], reg['n_other'], reg['n_disk'], len(EXCLUDED)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
