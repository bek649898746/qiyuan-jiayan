#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""verify_plan_steps —— 按 goal plan 的 ## Verification plan 逐条当场复算（开发级）。

设计: 每步只跑**已发布的产物 + 已提交的脚本**, 不重建编译器、不硬编码结果;
重步（自举 / --fast / run_criteria / D44 sweep）不在此重跑, 只核对**当前轮**已落盘的日志结论。

2026-10-24 加固（判据方打回「plan replay 读的是 10-23 旧快照」）:
  · 不动点 **从 `scripts/build_zh.ps1` 现读**（不再写死 4BDB13D2）；
  · 证据目录 **默认取 `grok-build/_evidence` 下最新一轮**（可用 --evidence 覆盖），不再钉死 `20261023_verify`；
  · 新增「交接表结构 + 当前不动点」判据：`_HANDOVER_*.md` 的 `## 六c` 段必须存在、
    且表内命令行 ≥10、且该段文本里出现**当前**不动点。

用法: python scripts/verify_plan_steps.py [--evidence <dir>]
退出: 0 = 计划里可当场复算的步骤全过; 1 = 有红
"""
import glob
import io
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


def main():
    ap_ev = ''
    if '--evidence' in sys.argv:
        ap_ev = sys.argv[sys.argv.index('--evidence') + 1]
    EV = ap_ev or os.environ.get('QCC_EVIDENCE') or newest_evidence()
    EXP = expected_fixed_point()
    fails = []
    print('=' * 78)
    print('不动点（现读 build_zh.ps1）: %s' % (EXP or '<缺失>'))
    print('证据目录（当前轮）        : %s' % (EV or '<未找到>'))
    if not EXP:
        fails.append('expected')

    # 步骤 2b / 1 / 判据行: 各判据脚本（真驱动已发布编译器）
    for name, cmd, want in [
        ('C1-ELMSIZE', ['python', 'scripts/check_c1_elmsize.py'], 0),
        ('R610', ['python', 'scripts/check_forward_global.py'], 0),
        ('PERF-DAG', ['python', 'scripts/check_perf_dag.py'], 0),
        ('D-UNION-PTR', ['python', 'scripts/check_d_union_ptr.py'], 0),
    ]:
        if not os.path.exists(os.path.join('scripts', cmd[1].split('/')[-1])):
            print('[%-18s] SKIP（脚本缺失）' % name)
            continue
        rc, out = run(cmd)
        ok = rc == want
        print('[%-18s] rc=%d %s' % (name, rc, 'PASS' if ok else 'FAIL'))
        if not ok:
            fails.append(name)
            for l in out.split('\n')[-4:]:
                if l.strip():
                    print('    ' + l.strip()[:140])

    # 台账一致性闸（读 HEAD 的 blob）
    rc, out = run(['python', os.path.join(LEDGER, 'scripts', 'check_tasklist.py')])
    print('[%-18s] rc=%d %s' % ('ledger gate', rc, 'PASS' if rc == 0 else 'FAIL'))
    if rc != 0:
        fails.append('ledger')

    # 目标状态闸（--plan 读当前 plan）
    plan = os.environ.get('GOAL_PLAN', '')
    if plan and os.path.exists(plan):
        rc, out = run(['python', os.path.join(LEDGER, 'scripts', 'check_goal_state.py'), '--plan', plan])
    else:
        rc, out = run(['python', os.path.join(LEDGER, 'scripts', 'check_goal_state.py')])
    print('[%-18s] rc=%d %s' % ('goal state gate', rc, 'PASS' if rc == 0 else 'FAIL'))
    if rc != 0:
        fails.append('goal-state')
        for l in out.split('\n')[-6:]:
            if l.strip():
                print('    ' + l.strip()[:150])

    # 交接表结构 + 当前不动点（本轮新增：判据方要求「复算当前交接表」而非旧快照）
    hs = sorted(glob.glob(os.path.join(LEDGER, '_HANDOVER_*.md')))
    if not hs:
        print('[%-18s] SKIP（无交接文档）' % 'handover table'); fails.append('handover')
    else:
        txt = io.open(hs[-1], encoding='utf-8').read()
        i = txt.find('## 六c')
        seg = txt[i:] if i >= 0 else ''
        rows = len(re.findall(r'^\| \d+ \|', seg, re.M))
        has_fp = EXP != '' and EXP in seg
        ok = i >= 0 and rows >= 10 and has_fp
        print('[%-18s] %s（%s · 命令行 %d · 含当前不动点=%s）'
              % ('handover table', 'PASS' if ok else 'FAIL', os.path.basename(hs[-1]), rows, has_fp))
        if not ok:
            fails.append('handover')

    # 交接表 §六c 第 1/4/5/6/7 行：**当场重跑**并断言各自的期望观察（判据方打回：
    #   「只数行数 + 查不动点」不等于复算；这里真驱动 shipped 编译器/判据脚本）。
    import tempfile
    tmp = tempfile.mkdtemp(prefix='vps_')
    csrc = os.path.join(tmp, 'r.c')
    io.open(csrc, 'w', encoding='utf-8', newline='\n').write(
        'int puts2(const char*); int main(void){ puts2("R"); return 0; }\n')
    live = [
        ('row1 --repeat', [os.path.join('.', 'qcc_zh_seed.exe'), '--repeat', '2', csrc, '-c', '-o', os.path.join(tmp, 'r.o')],
         r'\[REPEAT\] \u8fde\u7eed\u7f16\u8bd1'),
        ('row3 anchor811', ['python', 'scripts/run_tests_zh.py', '--only', r'scripts\feat_zh_ptrarr811_ok.c', '--parallel', '1'],
         r'PASS=1 SKIP=0 FAIL=0'),
        ('row3 anchor812', ['python', 'scripts/run_tests_zh.py', '--only', r'scripts\feat_zh_ptrarr812_ok.c', '--parallel', '1'],
         r'PASS=1 SKIP=0 FAIL=0'),
        ('row4 jyld smoke', ['python', 'scripts/check_jyld_link_smoke.py', 'qcc_zh_seed.exe'],
         r'\u5931\u8d25 0'),
        ('row5 split', ['python', 'scripts/check_split_unit.py', '--cc', 'qcc_zh_seed.exe'],
         r'D-SPLIT split unit: PASS'),
        ('row6 objmode self', ['python', 'scripts/check_objmode_self.py', '--cc', 'qcc_zh_seed.exe'],
         r'objmode self: PASS'),
        ('row7 runs-big', ['python', 'scripts/check_objmode_unit.py', '--runs-big', '--cc', 'qcc_zh_seed.exe'],
         r'R800 \u5224\u636e.*PASS'),
    ]
    for tag, cmd, pat in live:
        rc, out = run(cmd, timeout=900)
        ok = rc == 0 and re.search(pat, out) is not None
        print('[%-18s] rc=%d %s' % (tag, rc, 'PASS' if ok else 'FAIL'))
        if not ok:
            fails.append(tag)
            for l in out.split('\n')[-3:]:
                if l.strip():
                    print('    ' + l.strip()[:140])

    # 重步: 只核对**当前轮**已落盘日志的结论行（不重跑）
    checks = [
        ('bootstrap', os.path.join(EV, 'bootstrap.log'),
         (r'不动点 %s 达成' % EXP) if EXP else r'不动点 [0-9A-F]{8} 达成'),
        ('fast p1', os.path.join(EV, 'fast_p1.log'), r'PASS=\d+ SKIP=0 FAIL=0'),
        # 2026-10-24（第 6 轮 skeptic）：原来 `PASS \d+ / \d+` 允许 59/65 冒充全绿 ⟹
        # 改成「分子分母必须相等」，并强制出现 `FAIL = 无`（或 `FAIL=0`）。
        ('criteria fixed', os.path.join(EV, 'criteria_summary.log'),
         r'PASS (\d+) / \1(?!\d)\s*[；;]\s*FAIL = 无|FAIL=0'),
        ('d44 sweep', os.path.join(EV, 'd44_sweep.log'), r'O{20,}'),
    ]
    for tag, path, pat in checks:
        if not os.path.exists(path) and os.path.exists(os.path.join(os.path.dirname(path), 'logs', os.path.basename(path))):
            path = os.path.join(os.path.dirname(path), 'logs', os.path.basename(path))
        if not os.path.exists(path):
            print('[%-18s] SKIP（当前轮日志缺失: %s）' % (tag, os.path.basename(path)))
            fails.append(tag)
            continue
        ok = re.search(pat, read_text(path)) is not None
        print('[%-18s] %s（%s）' % (tag, 'PASS' if ok else 'FAIL', os.path.basename(path)))
        if not ok:
            fails.append(tag)

    print('=' * 78)
    print('计划步骤复算: %s%s' % ('全部通过' if not fails else '有红 ', '' if not fails else str(fails)))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
