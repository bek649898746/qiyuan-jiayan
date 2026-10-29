# -*- coding: utf-8 -*-
"""ORD-43 复现：IR 层四阶段（显式化/块/CFG+SSA/优化）尚不存在。blocked（战略级，需分阶段排期）。

结构判据（不依赖易漂行号）：
  ① 编译器 `--help` 无 IR 导出开关（`--ir`/`--dump-ir`/`--中间表示`）；
  ② 主源无 CFG/SSA 数据结构（`基本块`/`前驱`/`后继`/`支配`/`φ` 之类标识符全 0 命中）。
两项同时成立 ⟹ 「只有生成、没有修改能力」复现（rc=1，blocked 诚实）。
用法: python scripts/check_ir_layers.py [--cc qcc_zh_v1.exe]
"""
import argparse, io, os, re, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEYS = ['基本块', '前驱块', '后继块', '支配树', '支配边界', 'phi 节点', 'φ 节点', 'SSA']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    src = io.open(os.path.join(REPO, 'srclib_jiayan', 'qcc_zh.jy'), encoding='utf-8', errors='replace').read()
    hits = {k: len(re.findall(re.escape(k), src)) for k in KEYS}
    try:
        h = subprocess.run([cc, '--help'], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        help_txt = (h.stdout or '') + (h.stderr or '')
    except Exception as e:
        help_txt = ''
        print('[WARN] --help 失败: %s' % e)
    has_opt = any(k in help_txt for k in ('--ir', '--dump-ir', '--中间表示', '中间表示'))
    print('[SRC] CFG/SSA 关键词命中: %s' % hits)
    print('[OPT] --help 含 IR 导出开关: %s' % has_opt)
    core = (hits['基本块'] == 0 and hits['前驱块'] == 0 and hits['后继块'] == 0 and hits['支配边界'] == 0)
    if not has_opt and core:
        print('[REPRO] IR 四阶段 全缺（无导出开关 + 无 CFG/SSA 结构）⟹ blocked 诚实')
        return 1
    print('[PASS] 已出现 IR 结构或导出开关 ⟹ 应转 fixed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
