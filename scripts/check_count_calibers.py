# -*- coding: utf-8 -*-
"""check_count_calibers — D-COUNT 的**当前可复跑**口径互检（2026-10-21 虾米）

背景：台账原登记的 `scripts/check_count_calibers.py` **并不存在**（与 AI-D17 同类的
「登记名缺失」）。原判据是历史一次性的「三套口径互检」（291=291、366=116+250、净缺口=…），
在当前树上无法逐字复现。本脚本把「口径互检」固化成两条**会真失败**的一致性断言：

  ① 台账内部一致：`_BACKLOG_QUEUE.md` 的 **行数 == 各状态计数之和**
  ② 闸内引用一致：`run_tests_zh.py` 的 `CORE_FEAT` 里每个锚 **文件都存在**

用法: python scripts/check_count_calibers.py [--ledger <path>]
退出码: 0 PASS / 1 FAIL
"""
import argparse
import io
import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ledger', default=os.path.join(os.path.dirname(REPO), 'grok-build',
                                                     '_BACKLOG_QUEUE.md'))
    ap.add_argument('--tasklist', default=os.path.join(os.path.dirname(REPO), 'grok-build', '_TASKLIST.json'),
                    help='可指向「坏例」副本 ⟹ 判据必须报红')
    ap.add_argument('--gen-counts', default=os.path.join(os.path.dirname(REPO), 'grok-build', '_GEN_COUNTS.txt'))
    ap.add_argument('--mech', default=os.path.join(os.path.dirname(REPO), 'grok-build', '_data',
                                                   'mechanisms_291.txt'),
                    help='机制清单 v3.0（291 项）数据集；可指向「坏例」副本 ⟹ 判据必须报红')
    ap.add_argument('--map', default=os.path.join(os.path.dirname(REPO), 'grok-build', '_data',
                                                  'mechanisms_291_map.csv'),
                    help='三口径映射 CSV（num,key,src_hit,anchor_hit）；实时复算必须逐行一致')
    a = ap.parse_args()
    ok = True

    # ① 台账内部一致
    if os.path.exists(a.ledger):
        s = io.open(a.ledger, encoding='utf-8').read()
        rows = [l for l in s.split('\n') if l.startswith('|') and not l.startswith('|:')
                and not l.startswith('| ID')]
        cnt = {}
        for l in rows:
            cells = [x.strip() for x in l.strip('|').split('|')]
            for st in ('fixed', 'settled', 'blocked', 'open', 'in_progress'):
                if st in cells:
                    cnt[st] = cnt.get(st, 0) + 1
                    break
        tot = sum(cnt.values())
        good = (tot == len(rows))
        print('① 台账: rows=%d  状态和=%d  %s' % (len(rows), tot, cnt))
        print('   -> %s' % ('PASS' if good else 'FAIL'))
        ok &= good
    else:
        print('① 台账: 未找到 %s（跳过，不算失败）' % a.ledger)

    # ② CORE_FEAT 引用一致
    src = io.open(os.path.join('scripts', 'run_tests_zh.py'), encoding='utf-8', errors='replace').read()
    i = src.find('CORE_FEAT = [')
    j = src.find(']', i)
    block = src[i:j] if i >= 0 else ''
    anchors = re.findall(r"'(scripts/[^']+\.c)'", block)
    missing = [x for x in anchors if not os.path.exists(x)]
    print('② CORE_FEAT: %d 条；缺失文件 %d 个 %s' % (len(anchors), len(missing), missing[:5]))
    good2 = (not missing and len(anchors) > 0)
    print('   -> %s' % ('PASS' if good2 else 'FAIL'))
    ok &= good2

    # ③ 台账 JSON 与生成视图 一致（D-COUNT 2026-10-22: 台账已改为**生成式**，计数从 JSON 派生）
    tj = a.tasklist
    gc = a.gen_counts
    if os.path.exists(tj) and os.path.exists(gc):
        import json
        d = json.load(io.open(tj, encoding='utf-8'))
        rows2 = d.get('rows', d if isinstance(d, list) else [])
        cnt2 = {}
        for r in rows2:
            cnt2[r.get('status', '?')] = cnt2.get(r.get('status', '?'), 0) + 1
        line = io.open(gc, encoding='utf-8', errors='replace').read().strip().split('\n')[0]
        want = '%d = in_progress %d / open %d / blocked %d / fixed %d / settled %d' % (
            len(rows2), cnt2.get('in_progress', 0), cnt2.get('open', 0),
            cnt2.get('blocked', 0), cnt2.get('fixed', 0), cnt2.get('settled', 0))
        good3 = (line.strip() == want) and (cnt2.get('open', 0) == 0 and cnt2.get('in_progress', 0) == 0)
        print('③ _GEN_COUNTS 与 JSON 派生一致: 视图=%r 派生=%r  %s' % (line.strip(), want, 'PASS' if good3 else 'FAIL'))
        ok &= good3
        # ④ 每个 fixed 行：criterion_cmd 存在 + 脚本文件在 + last_rc==0
        bad = []
        for r in rows2:
            if r.get('status') != 'fixed':
                continue
            cmd = (r.get('criterion_cmd') or '').strip()
            if not cmd:
                bad.append((r['id'], 'criterion_cmd 空'))
                continue
            m = re.search(r'(scripts/[\w\-.]+)', cmd)
            if m and not os.path.exists(m.group(1)):
                bad.append((r['id'], '脚本缺失 %s' % m.group(1)))
            if r.get('last_rc') != 0:
                bad.append((r['id'], 'last_rc=%r' % r.get('last_rc')))
        print('④ fixed 行判据自洽: 不合格 %d 条 %s  %s' % (len(bad), bad[:5], 'PASS' if not bad else 'FAIL'))
        ok &= (not bad)
    else:
        print('③/④ 未找到 _TASKLIST.json / _GEN_COUNTS.txt（跳过，不算失败）')

    # ⑤ 机制清单 291（D-COUNT 2026-10-22：数据集**已重建入库** —— 原 291 件只存在于
    #    `IR层特性/_dbg_all291.txt`（审计快照外），仓内无主体 ⟹ 「三口径」无对象可查。
    #    现在主体在 `grok-build/_data/mechanisms_291.txt`（1..291 连续编号）。
    #    判据 = 行数 == 291 且 编号恰为 1..291（无缺/无重/无越界）。坏例（删一行/改编号）必须红。
    mech_nums = set()
    if os.path.exists(a.mech):
        ml = [l for l in io.open(a.mech, encoding='utf-8', errors='replace').read().split('\n') if l.strip()]
        nums = []
        for l in ml:
            m = re.match(r'\s*(\d+)', l)
            if m:
                nums.append(int(m.group(1)))
        mech_nums = set(nums)
        good5 = (len(ml) == 291 and len(nums) == 291 and mech_nums == set(range(1, 292)))
        print('⑤ 机制清单: 行=%d 编号=%d 覆盖面=%s(1..291)  %s'
              % (len(ml), len(nums), '完整' if mech_nums == set(range(1, 292)) else '有缺/重',
                 'PASS' if good5 else 'FAIL'))
        ok &= good5
    else:
        print('⑤ 机制清单: 未找到 %s ⟹ FAIL（数据集是判据主体，不可跳过）' % a.mech)
        ok = False

    # ⑥ 台账 level 的清单编号必须落在清单里（跨口径：JSON vs 291 清单）
    #    `level` 形如 `5/96` / `15/280` ⟹ 分母是清单编号；`—`/纯序号 不参与。
    if os.path.exists(a.tasklist) and mech_nums:
        import json as _json
        _d = _json.load(io.open(a.tasklist, encoding='utf-8'))
        _rows = _d.get('rows', _d if isinstance(_d, list) else [])
        bad6 = []
        for r in _rows:
            lv = str(r.get('level', '')).strip()
            m = re.match(r'^[^/]+/(\d+)$', lv)
            if not m:
                continue
            n = int(m.group(1))
            if n not in mech_nums:
                bad6.append((r['id'], lv))
        print('⑥ 台账 level → 清单编号: 越界 %d 条 %s  %s'
              % (len(bad6), bad6[:5], 'PASS' if not bad6 else 'FAIL'))
        ok &= (not bad6)

    # ⑦ 三口径映射 CSV：**实时复算**必须与仓库里的 CSV 逐行一致（291 行）
    #    口径 A = 清单 291 项；B = 关键词在 qcc_zh.jy 命中；C = 关键词在 feat_zh_*.c 命中。
    #    改清单（删项/改标题）或改源码/锚（删关键词）⟹ CSV 不一致 ⟹ 红。这不是「台账自洽」，是**计数主体对账**。
    if os.path.exists(a.map):
        import subprocess
        import tempfile
        tmp = os.path.join(tempfile.mkdtemp(prefix='mechmap_'), 'map.csv')
        r7 = subprocess.run([sys.executable, 'scripts/gen_mechanism_map.py', '--out', tmp,
                             '--mech', a.mech],
                            cwd=REPO, capture_output=True, text=True,
                            encoding='utf-8', errors='replace')
        if r7.returncode != 0:
            print('⑦ 三口径映射: 复算失败 rc=%d %s  FAIL' % (r7.returncode, (r7.stderr or '')[:160]))
            ok = False
        else:
            got = io.open(tmp, encoding='utf-8').read().split('\n')
            want = io.open(a.map, encoding='utf-8').read().split('\n')
            rows7 = len([x for x in got if x.strip()]) - 1
            same7 = (got == want)
            firstdiff7 = ''
            if not same7:
                for i in range(min(len(got), len(want))):
                    if got[i] != want[i]:
                        firstdiff7 = '首个差异 行%d: 现算=%r 仓库=%r' % (i + 1, got[i][:80], want[i][:80])
                        break
                else:
                    firstdiff7 = '行数不同 现算=%d 仓库=%d' % (len(got), len(want))
            good7 = same7 and rows7 == 291
            print('⑦ 三口径映射: 行=%d 逐行一致=%s  %s %s' % (rows7, same7,
                                                          'PASS' if good7 else 'FAIL', firstdiff7))
            ok &= good7
    else:
        print('⑦ 三口径映射: 未找到 %s ⟹ FAIL' % a.map)
        ok = False

    print('=== D-COUNT 口径互检: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
