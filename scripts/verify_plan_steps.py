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


def evidence_sync(ledger, logdir):
    """逐行核对：证据日志是否**对应当前台账判据**（防止旧判据的绿被当成本轮绿）。

    · 硬要求：fixed 必须有 `crit_<ID>.log`；blocked 必须有 `blocked_<ID>.log`；
    · **凡是存在的**该行日志（crit_ / blocked_ 两种前缀都算），首行命令都必须与当前
      `criterion_cmd` 一致 —— 否则审计可能读到一份「旧判据的绿」，这正是 skeptic 反复打的点。
    """
    p = os.path.join(ledger, '_TASKLIST.json')
    if not os.path.exists(p):
        return [], ['<无台账>']
    rows = json.load(io.open(p, encoding='utf-8'))['rows']
    stale, miss = [], []
    for r in rows:
        st = r.get('status')
        if st not in ('fixed', 'blocked', 'settled'):
            continue
        rid = r['id'].replace('/', '_')
        crit = os.path.join(logdir, 'crit_%s.log' % rid)
        blk = os.path.join(logdir, 'blocked_%s.log' % rid)
        if st == 'fixed' and not os.path.exists(crit):
            miss.append(r['id'])
        if st == 'blocked' and not os.path.exists(blk):
            miss.append(r['id'])
        for f in (crit, blk):
            if not os.path.exists(f):
                continue
            # 日志头允许有 `#` 说明行（取证/口径注记），命令取**第一条 `$ ` 行**。
            cmdline = next((l for l in read_text(f).splitlines() if l.startswith('$ ')), '')
            if cmdline[2:].strip() != (r.get('criterion_cmd') or '').strip():
                stale.append('%s←%s' % (r['id'], os.path.basename(f)))
        # 2026-10-27（skeptic 第十一轮）：**blocked 行不许留绿的 crit 日志**。
        # 场景：SUGAR-HUOFAN-EXPR 从 fixed 退回 blocked 后，`crit_<ID>.log` 仍是当年的 rc=0，
        # 审计按路径一读就是「绿」，而 blocked 的真相在 `blocked_<ID>.log`（rc=1）。
        # （settled 行例外：它们是「已执行并被结算」的测试资产，绿的 crit 日志属正常。）
        if st == 'blocked' and os.path.exists(crit):
            m = re.search(r'\[exit=(-?\d+)', read_text(crit))
            if m and int(m.group(1)) == 0:
                stale.append('%s←%s（blocked 行仍有 rc=0 的 crit 日志）'
                             % (r['id'], os.path.basename(crit)))
        # 修前腿 `crit_<ID>_before.log`：同一支判据 + 额外参数 ⟹ 允许**前缀**匹配
        bef = os.path.join(logdir, 'crit_%s_before.log' % rid)
        if os.path.exists(bef):
            cmdline = next((l for l in read_text(bef).splitlines() if l.startswith('$ ')), '')
            crit0 = (r.get('criterion_cmd') or '').strip()
            if crit0 and not cmdline[2:].strip().startswith(crit0):
                stale.append('%s←%s' % (r['id'], os.path.basename(bef)))
    return stale, miss


def main():
    EV = os.environ.get('QCC_EVIDENCE') or newest_evidence()
    LOG = os.path.join(EV, 'logs')
    EXP = expected_fixed_point()
    fails = []
    print('=' * 78)
    print('不动点（现读 build_zh.ps1）: %s' % (EXP or '<缺失>'))
    print('证据目录（当前轮）        : %s' % (EV or '<未找到>'))

    # 1) 清点：25 份施工单 ↔ 台账 ORD-* 身份一一对应
    #    2026-10-27（skeptic 第七轮）：证据必须**本次运行即落盘** —— 旧版只读上次的
    #    `orders_census.log`，而它捕获在 ORD-14 还是 settled 的那天（fixed 5/19/1），
    #    与 README/台账（6/18/1）说了两套账。现在每次复算都重写该日志。
    rc, out = run(['python', 'scripts/check_orders_census.py',
                   '--out', os.path.join(LOG, 'orders_census.log')])
    need(fails, 'orders census', rc == 0, out.splitlines()[-1][:90] if out else '')

    # 2) 修前红 / 修后绿 —— **scope = 施工单 25 份**（不是台账全部 159 行）。
    #
    # 口径（2026-10-27 第六轮，skeptic 打回「一边自报 2/117 一边算通过」后写死）：
    #   plan criterion 2 的主语是**本轮递的施工单**，所以本步逐行校验 = 台账里 `ORD-` 前缀且 fixed 的行；
    #   每条都必须同时有 `crit_<ID>_before.log`（rc≠0）与 `crit_<ID>.log`（rc=0），
    #   且两份日志的首个 `$ ` 行都以台账 `criterion_cmd` 开头（同一支判据，只换修前/修后产物或输入）。
    #   台账其余的历史 fixed 行**不在本 goal 的 25 单范围内**，它们的修前红留在各自那一轮的
    #   `_evidence/<round>/`；本闸不对它们主张「已绑定」，也不把它们算进覆盖率分母。
    rows_all = json.load(io.open(os.path.join(LEDGER, '_TASKLIST.json'), encoding='utf-8'))['rows']
    ord_fixed = [r for r in rows_all if r['id'].startswith('ORD-') and r.get('status') == 'fixed']
    hist_fixed = [r['id'] for r in rows_all if r.get('status') == 'fixed' and not r['id'].startswith('ORD-')]
    miss_b, miss_a, diff = [], [], []
    for r in ord_fixed:
        rid = r['id'].replace('/', '_')
        bf = os.path.join(LOG, 'crit_%s_before.log' % rid)
        af = os.path.join(LOG, 'crit_%s.log' % rid)
        crit = (r.get('criterion_cmd') or '').strip()
        for path, bucket, want in ((bf, miss_b, '!=0'), (af, miss_a, '==0')):
            if not os.path.exists(path):
                bucket.append(r['id'])
                continue
            txt = read_text(path)
            m = re.search(r'\[exit=(-?\d+)', txt)
            rc_v = int(m.group(1)) if m else None
            if (want == '!=0' and rc_v in (0, None)) or (want == '==0' and rc_v != 0):
                bucket.append('%s(rc=%s)' % (r['id'], rc_v))
        for path in (bf, af):
            if not os.path.exists(path):
                continue
            cl = next((l for l in read_text(path).splitlines() if l.startswith('$ ')), '')
            if crit and not cl[2:].strip().startswith(crit):
                diff.append(os.path.basename(path))

    ok2 = not (miss_b or miss_a or diff)
    need(fails, 'before red', ok2,
         'scope=施工单 25 份（fixed %d 条：%s）；缺 before=%s 缺 after=%s 非同源=%s；'
         '非 ORD 的 fixed 行 %d 条不按「施工单」计（历史轮次 + 本轮的闸自测行），它们的判据由 run_criteria 全量跑'
         % (len(ord_fixed), ', '.join(r['id'] for r in ord_fixed),
            miss_b or '无', miss_a or '无', diff or '无', len(hist_fixed)))
    print('[%-18s] INFO 逐行绑定 %d/%d（=ORD fixed 条数）；非 ORD fixed %d 条不计入分母'
          % ('criterion2 scope', len(ord_fixed) - len(set(miss_b) | set(miss_a)), len(ord_fixed),
             len(hist_fixed)))

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
            # 2026-10-30：不动点 = **相邻两代全等**，不必是 v1==v2（本轮两刀都需 3 代才稳定）
        _seq = sorted((int(n), h) for n, h in re.findall(r'v(\d+) = ([0-9A-F]{8})', t))
        _hs = [h for _n, h in _seq]
        _conv = len(_hs) >= 2 and _hs[-1] == _hs[-2] and _hs[-1] == EXP
        ok = (_conv
              and 'PASS' in t
              and '中文变量名赋值行: 0' in p0
              and 'PARSE_OK' in p0
              and 'NOT_UTF8(UTF-16 BOM)' in p0)
        need(fails, 'bootstrap', ok,
             '最后两代全等 == %s（链 %s）+ Parser0/中文0/UTF16红'
             % (EXP, '→'.join(_hs) or '<缺>'))

    # 5) 读行 6 锚 + --serve 往返（真跑）
    anchors = ('scripts/feat_zh_io_readline_ok.c,scripts/feat_zh_neg_a6014.c,'
               'scripts/feat_zh_neg_a6059_import.c,scripts/feat_zh_neg_a6059_export.c,'
               'scripts/feat_zh_neg_a3005.c,scripts/feat_zh_neg_a6016.c')
    rc, out = run(['python', 'scripts/run_tests_zh.py', '--only', anchors, '--parallel', '2'])
    need(fails, 'readline anchors', rc == 0 and 'PASS=6 SKIP=0 FAIL=0' in out, 'PASS=6')

    # 6) 不回退：全量判据汇总（当前轮日志）
    #    汇总**必须自报来源**：批跑 N 行 + 单独重跑 M 行。M>0 时它是「复合结论」，
    #    本闸不许把它当成「一次干净全量 run」（skeptic：console 116/117 + 单跑改写 = 两套账）。
    cs = os.path.join(LOG, 'criteria_summary.log')
    if not os.path.exists(cs):
        need(fails, 'criteria', False, '缺 logs/criteria_summary.log')
    else:
        t = read_text(cs)
        ok = re.search(r'PASS (\d+) / \1(?!\d)\s*[；;]\s*FAIL = 无', t) is not None
        ok = ok and 'STALE =' not in t and 'MISSING =' not in t
        src = re.search(r'# 来源[：:].*?批跑 (\d+) 行.*?(\d+) 行', t)
        rows_n = len([l for l in t.splitlines()
                      if l.startswith('| ') and not l.startswith('| ID') and not l.startswith('|:')])
        comp = ''
        if not src:
            ok = False
            comp = '缺「# 来源」自报行'
        elif int(src.group(1)) + int(src.group(2)) != rows_n:
            ok = False
            comp = '来源行 %s+%s ≠ 表内 %d 行' % (src.group(1), src.group(2), rows_n)
        else:
            comp = '复合（批跑 %s + 单独重跑 %s）' % (src.group(1), src.group(2)) \
                if int(src.group(2)) else '一次批跑 %s 行' % src.group(1)
        need(fails, 'criteria', ok, 'PASS N/N + 无 STALE/MISSING + 来源自报；%s' % comp)

    # 6b) 证据同源（skeptic：曾有 crit 日志属旧判据 ⟹ 汇总里的绿是旧绿）
    #     fixed 行必须有 crit_<ID>.log 且首行命令 == 当前 criterion_cmd；
    #     blocked 行必须有 blocked_<ID>.log 且首行命令同源。
    stale, miss = evidence_sync(LEDGER, LOG)
    need(fails, 'evidence sync', not stale and not miss,
         '逐行日志 ↔ 台账判据；STALE=%s MISSING=%s' % (stale or '无', miss or '无'))

    # 6c) 「现役 / 实时账」邻近不许手抄易变值
    #     第十二轮的打回：FINAL_RESPONSE 消息把 R888 的数写成「现役账（R888：163 = 119 fixed…）」
    #     —— R889 收口后坐标前移，同一句话又成了旧账。根因是「复述机器能算的数」这个动作本身，
    #     所以判据不是「这次改对」，而是「以后不许这么写」（含反例自测）。
    gate = os.path.join(LEDGER, 'scripts', 'check_superseded_claims.py')
    rc_self, out_self = run([sys.executable, gate, '--selftest'])
    rc_gate, out_gate = run([sys.executable, gate])
    lines_s = [l for l in out_self.splitlines() if l.strip()]
    lines_g = [l for l in out_gate.splitlines() if l.strip()]
    # 条数从闸自己的输出转发，**不在本闸里复述**（写死数字就会在加例后变成假话）
    need(fails, 'live-claim gate', rc_self == 0 and rc_gate == 0,
         '%s；%s' % (lines_s[-1] if lines_s else '', lines_g[-2] if len(lines_g) >= 2 else ''))

    # 7) 证据与工作区口径
    need(fails, 'evidence', os.path.exists(os.path.join(EV, 'README.md')) and
         os.path.exists(os.path.join(EV, 'FINAL_RESPONSE.md')), 'README+FINAL_RESPONSE')
    bad = []
    for f in glob.glob(os.path.join(LOG, '*.log')):
        b = open(f, 'rb').read(4)
        if b[:2] in (b'\xff\xfe', b'\xfe\xff'):
            bad.append(os.path.basename(f))
    need(fails, 'logs utf8', not bad,
         '证据日志一律 UTF-8（PowerShell Tee-Object 的 UTF-16 会二次编码中文）; UTF16=%s' % (bad or '无'))
    rc, out = run(['python', os.path.join(LEDGER, 'scripts', 'check_worktree_status.py')])
    need(fails, 'worktree', rc == 0, '跟踪改动 == 0')

    print('=' * 78)
    print('计划步骤复算: %s%s' % ('全部通过' if not fails else '有红 ', '' if not fails else str(fails)))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
