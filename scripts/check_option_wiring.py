# -*- coding: utf-8 -*-
"""check_option_wiring —— 选项接线判据（三态：WIRED / ORPHAN / MISSING）。

背景（施工单《负锚三缺口与选项接线判据》）：
  `--serve` 的测试 (`_verify_serve.py`) 写得对，但**全仓没有调用者** ⟹ 测试存在、正确、从不执行。
  「有判据」与「判据上了流水线」在文件系统上长得一样 ⟹ 必须分开报。

判据：
  WIRED   = 有判据脚本，且该脚本在 CI/判据链上被实际执行
  ORPHAN  = 有判据脚本，但没人调用它  ⟹ 红
  MISSING = 连判据脚本都没有         ⟹ 红

用法: python scripts/check_option_wiring.py [--cc exe] [--json]
退出码: 0 = 无 ORPHAN 且无 MISSING; 1 = 有
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

# 判据链的根：CI + 台账仓的 run_criteria 入口
CI = '.github/workflows/ci.yml'
CRITERIA = os.path.join(os.path.dirname(REPO), 'grok-build', 'scripts', 'run_criteria.py')


def read(p):
    try:
        return open(p, encoding='utf-8', errors='replace').read()
    except Exception:
        return ''


def root_scripts():
    """CI 里直接执行的脚本 + 判据链入口里引用的脚本。"""
    seeds = set(re.findall(r'python\s+(scripts/[\w./-]+\.py)', read(CI)))
    # 台账仓 run_criteria 的命令字段（criterion_cmd）里出现的 scripts/xxx.py
    seeds |= set(re.findall(r'scripts[/\\]([\w./-]+\.py)', read(CRITERIA)))
    # 统一成相对编译器仓的路径
    return set(('scripts/' + s) if not s.startswith('scripts/') else s for s in seeds)


def expand(seeds, depth=3):
    seen = set(seeds)
    frontier = set(seeds)
    for _ in range(depth):
        nxt = set()
        for s in frontier:
            body = read(s)
            for m in re.findall(r'scripts[/\\]([\w./-]+\.py)', body):
                p = 'scripts/' + m
                if p not in seen:
                    nxt.add(p)
        seen |= nxt
        frontier = nxt
    return seen


def real_call(opt, text):
    """判据脚本里是否真的把该选项作为子进程实参传过。"""
    return re.search(r"(?:\bPopen|\bsubprocess\.run|\bcheck_output|\brun)\s*\(\s*\[[^\]]*['\"]" +
                     re.escape(opt) + r"['\"]", text) is not None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_seed.exe')
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)

    h = subprocess.run([cc, '--help'], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', cwd=REPO)
    opts = sorted(set(re.findall(r'--[a-zA-Z][\w-]*', h.stdout or '')))
    if not opts:
        print('[FAIL] --help 没有解析出任何选项（编译器不可用?）')
        return 1

    wired_scripts = expand(root_scripts())
    all_scripts = set(p.replace('\\', '/') for p in glob.glob('scripts/*.py'))
    wired_text = '\n'.join(read(p) for p in sorted(wired_scripts))
    all_text = '\n'.join(read(p) for p in sorted(all_scripts))

    rows = []
    for o in opts:
        if real_call(o, wired_text):
            st = 'WIRED'
        elif real_call(o, all_text):
            st = 'ORPHAN'
        else:
            st = 'MISSING'
        rows.append((st, o))

    if a.json:
        print(json.dumps([{'state': s, 'option': o} for s, o in rows], ensure_ascii=False, indent=1))
    else:
        print('选项接线判据（%d 个选项 / 判据链脚本 %d 个）' % (len(opts), len(wired_scripts)))
        for st in ('WIRED', 'ORPHAN', 'MISSING'):
            print('--- %s (%d) ---' % (st, sum(1 for s, _ in rows if s == st)))
            for s, o in rows:
                if s == st:
                    print('  %-8s %s' % (s, o))

    bad = [o for s, o in rows if s != 'WIRED']
    print('\n结论: WIRED=%d ORPHAN=%d MISSING=%d' % (
        sum(1 for s, _ in rows if s == 'WIRED'),
        sum(1 for s, _ in rows if s == 'ORPHAN'),
        sum(1 for s, _ in rows if s == 'MISSING')))
    if '--serve' in opts:
        st = dict((o, s) for s, o in rows).get('--serve')
        print('--serve: %s' % st)
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
