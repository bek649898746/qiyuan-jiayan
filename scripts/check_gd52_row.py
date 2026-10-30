# -*- coding: utf-8 -*-
"""check_gd52_row —— 施工单《G-D52-PERF 状态改回 blocked》的**验收闸**（逐条对 §四）。

为什么长这样（2026-10-27 第四轮返工）：
  上一版把判据改成「台账状态 ↔ 实测余量一致」，好让 `settled + 4.5%` 通过 —— 那是**改判据去迁就状态**，
  正好把施工单 §四.1（状态必须 blocked）和 §四.3（pending_signoff 里不许出现本行）架空。
  现在闸只认**单子的原文**，余量只作记录、不参与判定：余量恢复到 4.5% 也不能自行改 settled，
  那是「裁定要不要撤销」的问题，得大哥重新裁定。

判据（= 施工单 §四；2026-10-28 大哥裁定「撤监视」后改成**实质四条**）：
  ① `_TASKLIST.json` 的 `G-D52-PERF` 行 status ∈ {`blocked`, `settled`}，且 evidence 非空；
     —— **settled 必须带「裁定」记录**，否则就是自己改状态绕过裁定（与第三轮同型的错）；
  ② `_LONG_MARCH_TASKLIST.md` 视图同一行也是同一个 status（视图与台账不许分叉）；
  ③ `pending_signoff`（= settled 且 next_step 含「签字/拍板」）里**不出现** G-D52-PERF；
  ④ `scripts/check_compile_speed.py` 的 `git diff` 为空（「判据不动」）。

为什么不再写死 `blocked`：原第 ① 条把状态锁在施工单当时的要求上，裁定一撤，闸就变成
「拦住正确的新状态」；而放宽成「任意状态通过」又会架空 §四.1。所以改成「状态必须是这两个之一
+ settled 必须附裁定记录 + 视图不分叉」—— 仍抓「用改状态绕过裁定」，但不再替大哥做裁定。

另附（只打印，不判）：最近一次速度取证的 `-c` best 与余量。

用法:
  python scripts/check_gd52_row.py                       # 当前台账
  python scripts/check_gd52_row.py --ledger <旧台账目录>   # 修前红取证（旧提交的 worktree）
"""
import argparse
import glob
import io
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'scripts'))
from check_gd52_margin import MONITOR_MARGIN_PCT, LIMIT_C  # 阈值单一来源


def read_text(path):
    """证据日志可能来自 python（UTF-8）或 PowerShell Tee-Object（UTF-16 + BOM）——按 BOM 解码。"""
    b = open(path, 'rb').read()
    if b[:2] in (b'\xff\xfe', b'\xfe\xff'):
        return b.decode('utf-16', errors='replace')
    return b.decode('utf-8', errors='replace')


def newest_margin_log(ledger):
    """规范名优先：最新证据目录里的 `logs/gd52_margin.log`（当前口径）；不按 mtime 选。"""
    ds = sorted(glob.glob(os.path.join(ledger, '_evidence', '2026*')), reverse=True)
    for d in ds:
        p = os.path.join(d, 'logs', 'gd52_margin.log')
        if os.path.exists(p):
            return p
    for d in ds:
        p = os.path.join(d, 'logs', 'blocked_G-D52-PERF.log')
        if os.path.exists(p):
            return p
    return ''


def parse_best(txt):
    """只取数字：`[-c] … best=105.0s`（日志若被二次编码，ASCII 段仍然可读）。"""
    m = re.search(r'\[-c\][^\n]*?best[=\s]([\d.]+)', txt) or re.search(r'best[=\s]([\d.]+)\s*s', txt)
    return float(m.group(1)) if m else None


def pending_signoff(rows):
    """与 gen_goal_state 同口径：settled 且 next_step 里写着「签字/拍板」。"""
    return [r['id'] for r in rows if r['status'] == 'settled'
            and any(k in (r.get('next_step') or '') for k in ('签字', '拍板'))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ledger', default=os.path.join(os.path.dirname(REPO), 'grok-build'))
    a = ap.parse_args()
    LEDGER = os.path.abspath(a.ledger)
    print('ledger=%s' % LEDGER)
    fails = []

    d = json.load(io.open(os.path.join(LEDGER, '_TASKLIST.json'), encoding='utf-8'))
    rows = d['rows']
    row = next((r for r in rows if r['id'] == 'G-D52-PERF'), None)

    # ① 状态 blocked + evidence 非空
    if not row:
        fails.append('_TASKLIST.json 缺 G-D52-PERF')
        status = ''
    else:
        status = row['status']
        if status not in ('blocked', 'settled'):
            fails.append('§四.1 status=%s（只允许 blocked / settled）' % status)
        if not (row.get('evidence') or '').strip():
            fails.append('§四.1 evidence 为空（状态变更需可复算理由）')
        if status == 'settled' and '裁定' not in (row.get('evidence') or ''):
            fails.append('§四.1 status=settled 但 evidence 无「裁定」记录（不许自行改状态绕过裁定）')

    # ② 视图一致
    lm = os.path.join(LEDGER, '_LONG_MARCH_TASKLIST.md')
    if not os.path.exists(lm):
        fails.append('缺 _LONG_MARCH_TASKLIST.md')
    elif status:
        hit = [l for l in io.open(lm, encoding='utf-8', errors='replace').read().splitlines()
               if 'G-D52-PERF' in l and l.strip().startswith('|')]
        if not hit:
            fails.append('_LONG_MARCH_TASKLIST.md 无 G-D52-PERF 表行')
        elif not any(re.search(r'\|\s*%s\s*\|' % status, l) for l in hit):
            fails.append('视图与台账分叉：_LONG_MARCH 行不是 %s' % status)

    # ③ pending_signoff 里不许出现本行
    pend = pending_signoff(rows)
    if 'G-D52-PERF' in pend:
        fails.append('§四.3 pending_signoff 含 G-D52-PERF（单子明确要求不出现）')

    # ④ 判据不动
    r = subprocess.run(['git', 'diff', '--quiet', '--', 'scripts/check_compile_speed.py'], cwd=REPO)
    if r.returncode != 0:
        fails.append('§四.4 check_compile_speed.py 有改动（违反「判据不动」）')

    # 附：余量只作记录
    ml = newest_margin_log(LEDGER)
    if ml:
        best = parse_best(read_text(ml))
        if best is not None:
            print('附（不参与判定）: -c best=%.1fs / 阈值 %.1fs ⟹ 余量 %.1f%%（监视阈值 %.1f%%）'
                  % (best, LIMIT_C, (LIMIT_C - best) / LIMIT_C * 100.0, MONITOR_MARGIN_PCT))
            if (LIMIT_C - best) / LIMIT_C * 100.0 >= MONITOR_MARGIN_PCT:
                print('   ⟹ 余量已 ≥ 阈值：%s'
                      % ('状态已按裁定置 settled，本行仅留历史记录。' if status == 'settled'
                         else 'blocked 的「余量不足」理由过期，但**状态仍按单维持**，等裁定。'))

    if fails:
        print('[FAIL] G-D52-PERF 施工单 §四: ' + '; '.join(fails))
        return 1
    print('[PASS] G-D52-PERF 施工单 §四 四条全成立：status=%s（∈{blocked,settled}）/ 视图一致 / '
          'pending_signoff 无本行 / 判据 diff 空%s'
          % (status, '（settled 已附裁定记录）' if status == 'settled' else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
