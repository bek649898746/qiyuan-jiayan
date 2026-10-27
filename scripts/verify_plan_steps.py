#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""verify_plan_steps —— 按**当前** goal plan 的 ## Verification plan 逐条当场复算。

2026-10-27 重写（上一版硬编码 10-26 的 bootstrap/fast_p1/d44 三步，已与当前计划脱节）：
当前计划的 7 步 = 清点 / 修前红-修后绿 / 永久闸接线 / 自举入口 /
读行与 --serve / 不回退 / 证据与工作区口径。

设计：重步（自举 / 全量判据）不重跑，只核对**当前轮已落盘日志的结论行**；
轻步（清点/接线/读行/serve）真跑已发布脚本。
"""
import glob
import io
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
LEDGER = os.path.join(os.path.dirname(REPO), 'grok-build')


def run(cmd, timeout=900):
    p = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                       errors='replace', timeout=timeout)
    return p.returncode, ((p.stdout or '') + (p.stderr or '')).strip()


def expected_fixed_point():
    t = io.open(os.path.join('scripts', 'build_zh.ps1'), encoding='utf-8', errors='replace').read()
    m = re.search(r"^\$expected = '([0-9A-Fa-f]{8})'", t, re.M)
    return m.group(1).upper() if m else ''


def newest_evidence():
    dirs = [d for d in glob.glob(os.path.join(LEDGER, '_evidence', '2026*')) if os.path.isdir(d)]
    return max(dirs) if dirs else ''


def read_text(path):
    fb = open(path, 'rb').read()
    if fb[:2] in (b'\xff\xfe', b'\xfe\xff'):
        return fb.decode('utf-16', errors='replace')
    return fb.decode('utf-8', errors='replace')


def need(fails, tag, ok, extra=''):
    print('[%-18s] %s%s' % (tag, 'PASS' if ok else 'FAIL', ('（%s）' % extra) if extra else ''))
    if not ok:
        fails.append(tag)


def main():
    EV = os.environ.get('QCC_EVIDENCE') or newest_evidence()
    LOG = os.path.join(EV, 'logs')
    EXP = expected_fixed_point()
    fails = []
    print('=' * 78)
    print('不动点（现读 build_zh.ps1）: %s' % (EXP or '<缺失>'))
    print('证据目录（当前轮）        : %s' % (EV or '<未找到>'))

    # 1) 清点：25 份施工单 ↔ 台账 ORD-* 身份一一对应
    rc, out = run(['python', 'scripts/check_orders_census.py'])
    need(fails, 'orders census', rc == 0, out.splitlines()[-1][:90] if out else '')

    # 2) 修前红 / 修后绿（当前轮 3 个 fixed 单：ORD-01/02/08；pure-gate/anchor 已按例外记 settled）
    def has(name, pat):
        p = os.path.join(LOG, name)
        return os.path.exists(p) and re.search(pat, read_text(p)) is not None
    need(fails, 'before red',
         has('readline_before_run.log', r'RL1 0 0') and
         has('serve_before.log', r'\[FAIL\]') and
         has('bootstrap_before.log', r'中文变量名赋值行: 343') and
         has('bootstrap_before.log', r'PARSE_FAIL') and
         has('prelude_before.log', r'A1001') and
         has('prelude_before.log', r'EOF'), 'ORD-01/02/08 修前')
    need(fails, 'after green',
         has('readline_after_run.log', r'RL1 6 1') and
         has('serve_after.log', r'\[PASS\]') and
         has('p0_bootstrap.log', r'中文变量名赋值行: 0') and
         has('p0_bootstrap.log', r'PARSE_OK') and
         has('p0_bootstrap.log', r'NOT_UTF8\(UTF-16 BOM\)') and
         has('prelude_after.log', r'\[PASS\]'), 'ORD-01/02/08 修后')

    # 3) 永久闸接线：--serve 判据真跑且已 WIRED
    rc, out = run(['python', 'scripts/check_serve.py'])
    need(fails, 'serve wired', rc == 0 and '[PASS]' in out, 'check_serve.py')
    rc, out = run(['python', 'scripts/check_option_wiring.py'])
    need(fails, 'option wiring', '--serve: WIRED' in out, 'check_option_wiring.py')

    # 4) 自举入口：v1==v2 + Parser 0 错 / 中文死赋值 0 / UTF-16 报红（p0_bootstrap.log）
    bp = os.path.join(LOG, 'bootstrap.log')
    p0p = os.path.join(LOG, 'p0_bootstrap.log')
    if not (os.path.exists(bp) and os.path.exists(p0p)):
        need(fails, 'bootstrap', False, '缺 logs/bootstrap.log 或 p0_bootstrap.log')
    else:
        t = read_text(bp)
        p0 = read_text(p0p)
        ok = (('v1 = %s' % EXP) in t and ('v2 = %s' % EXP) in t and 'PASS' in t and
              '中文变量名赋值行: 0' in p0 and 'PARSE_OK' in p0 and
              'NOT_UTF8(UTF-16 BOM)' in p0)
        need(fails, 'bootstrap', ok, 'v1==v2==%s + Parser0/中文0/UTF16红' % EXP)

    # 5) 读行 6 锚 + --serve 往返（真跑）
    anchors = ('scripts/feat_zh_io_readline_ok.c,scripts/feat_zh_neg_a6014.c,'
               'scripts/feat_zh_neg_a6059_import.c,scripts/feat_zh_neg_a6059_export.c,'
               'scripts/feat_zh_neg_a3005.c,scripts/feat_zh_neg_a6016.c')
    rc, out = run(['python', 'scripts/run_tests_zh.py', '--only', anchors, '--parallel', '2'])
    need(fails, 'readline anchors', rc == 0 and 'PASS=6 SKIP=0 FAIL=0' in out, 'PASS=6')

    # 6) 不回退：全量判据汇总（当前轮日志）
    cs = os.path.join(LOG, 'criteria_summary.log')
    if not os.path.exists(cs):
        need(fails, 'criteria', False, '缺 logs/criteria_summary.log')
    else:
        t = read_text(cs)
        ok = re.search(r'PASS (\d+) / \1(?!\d)\s*[；;]\s*FAIL = 无', t) is not None
        need(fails, 'criteria', ok, 'PASS N/N + FAIL=无')

    # 7) 证据与工作区口径
    need(fails, 'evidence', os.path.exists(os.path.join(EV, 'README.md')) and
         os.path.exists(os.path.join(EV, 'FINAL_RESPONSE.md')), 'README+FINAL_RESPONSE')
    rc, out = run(['python', os.path.join(LEDGER, 'scripts', 'check_worktree_status.py')])
    need(fails, 'worktree', rc == 0, '跟踪改动 == 0')

    print('=' * 78)
    print('计划步骤复算: %s%s' % ('全部通过' if not fails else '有红 ', '' if not fails else str(fails)))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
