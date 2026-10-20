#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""负例锚有效性审计（闸③ 候选 · 2026-10-01 第 439 站续四十四~四十六）

两轮审计的经验固化：

  ① 判据：compile_fail 只要求【退出码 ≠ 0】⟹ 任何不相干的错误都能让负例锚"通过"。
  ② 本轮实证：203 个 compile_fail 锚中，154 个（76%）无 @EXPECTED err: 断言；
     另有 6 个假锚 —— 它们**声称的码从未被验证**（命名撞保留词 / 形态不纯 / 判据被别的检查掩盖）。

本工具对每个 compile_fail 锚做**实测**，与它**声称的码**对比，分三档报告：

  [假锚]  注释头里恰好声称一个码 ∧ 实测码与它无交集   ⟹ 声称的规则**从未被验证**
  [弱锚]  无 @EXPECTED err: 断言                      ⟹ 只验"拒绝"，不验"拒绝理由"
  [说明]  实测码 ≠ 断言码（但都有）                    ⟹ 断言与判据不一致

用法:
  python scripts/check_neg_anchors.py                # 全量审计（约 4-6 分钟）
  python scripts/check_neg_anchors.py --quiet        # 只打印汇总与假锚
  python scripts/check_neg_anchors.py --json out.json
环境:
  QCC_TEST_BIN  编译器（默认 qcc_zh_v1.exe）
退出码: 有【假锚】⟹ 1；否则 0
"""

# R715 仪器刀 (2026-10-11 虾米): 默认控制台 GBK 时 emoji(✅/❌) 会让 print 抛 UnicodeEncodeError
#   ⟹ 检查器 rc=1 假红 ⟹ 闸⑥/三套件 被中断。强推 UTF-8 输出（Python 3.7+，非 tty 管道下同样生效），
#   不改任何 print 文本；调用侧（verify.ps1 / run_tests_zh.py）另设 PYTHONIOENCODING=utf-8 作兜底。
import sys as _sys_utf8w
for _stream_utf8w in (getattr(_sys_utf8w, 'stdout', None), getattr(_sys_utf8w, 'stderr', None)):
    if _stream_utf8w is not None and hasattr(_stream_utf8w, 'reconfigure'):
        _stream_utf8w.reconfigure(encoding='utf-8', errors='replace')
import io
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
BIN = os.environ.get('QCC_TEST_BIN', 'qcc_zh_v1.exe')
if not os.path.exists(BIN):
    # AUDIT-P2-4 (2026-10-21): **环境前置** —— 缺编译器 ⟹ SKIP（rc=0 + `[SKIP]` 标记），
    # 不再在 subprocess 里抛 FileNotFoundError（那是「假红」）。
    print('[SKIP] 环境未就绪: 找不到 编译器 %s —— 未执行本闸' % BIN); sys.exit(0)
QUIET = '--quiet' in sys.argv
JSON_OUT = None
if '--json' in sys.argv:
    JSON_OUT = sys.argv[sys.argv.index('--json') + 1]


def gate_files():
    """闸内锚清单：CORE_FEAT / CORE_FEAT += 两个列表块中的【非注释行】条目。

    ⚠️ 不能用全文正则 —— 注释里也提到文件名（如 `# [359 站暂缓] 'scripts/xxx.c'` 曾被误计入）。
    """
    t = io.open('scripts/run_tests_zh.py', encoding='utf-8').read()
    out = []
    for marker in ('CORE_FEAT = [', 'CORE_FEAT += ['):
        i = t.find(marker)
        if i < 0:
            continue
        blk = t[i:t.find('\n]', i)]
        for line in blk.split('\n'):
            if line.lstrip().startswith('#'):
                continue
            out += re.findall(r"'(scripts/[^']+\.c)'", line)
    return sorted(set(out))


rows, fake, weak, mismatch = [], [], [], []
for p in gate_files():
    if not os.path.exists(p):
        continue
    src = io.open(p, encoding='utf-8-sig', errors='replace').read()
    base = os.path.basename(p)
    if not (('@EXPECTED compile_fail' in src) or ('_neg' in base)):
        continue
    has_err = bool(re.search(r'@EXPECTED\s+err:', src))
    run_time = bool(re.search(r'@EXPECTED\s+(exit|out):', src))
    # ⚠️ 只认【错误码】形式 A\d{4} —— 注释里的 `L4814`（行号引用）曾被误当成"声称码"⟹ 假警报
    claimed = sorted(set(re.findall(r'\b(A\d{4})\b', src[:2500])))
    r = subprocess.run([BIN, p, '-o', 'scratch_c1b2/_cna_tmp.exe'], capture_output=True)
    out = (r.stdout + r.stderr).decode('utf-8', 'replace')
    actual = sorted(set(re.findall(r'\[ERR\]\s*(A\d{4})', out)))
    err_code = (re.search(r'@EXPECTED\s+err:\s*(\S+)', src) or [None, None])[1]
    rec = dict(file=base, rc=r.returncode, claimed=claimed, actual=actual,
               has_err=has_err, err_code=err_code, run_time=run_time)
    rows.append(rec)
    if run_time:
        continue
    if len(claimed) == 1 and actual and not (set(claimed) & set(actual)):
        fake.append(rec)
    if not has_err:
        weak.append(rec)
    # ⚠️ 只在【断言就是错误码】时才比对 —— 断言可以是任意文本子串（如 `未声明标识符 'x'`）
    elif err_code and re.match(r'^A\d{4}$', err_code) and actual and err_code not in actual:
        mismatch.append(rec)

print('负例锚审计 · 编译器 %s' % BIN)
print('  compile_fail 锚      : %d' % len([x for x in rows if not x['run_time']]))
print('  ── [假锚] 声称码未被验证 : %d' % len(fake))
print('  ── [弱锚] 无 err: 断言   : %d' % len(weak))
print('  ── [说明] 断言码≠实测码  : %d' % len(mismatch))
if fake:
    print()
    print('假锚明细:')
    for x in fake:
        print('  %-54s 声称 %-8s 实测 %s' % (x['file'], ','.join(x['claimed']), ','.join(x['actual'])))
if not QUIET and mismatch:
    print()
    print('断言码 ≠ 实测码:')
    for x in mismatch:
        print('  %-54s 断言 %-8s 实测 %s' % (x['file'], x['err_code'], ','.join(x['actual'])))
if JSON_OUT:
    json.dump(dict(rows=rows, fake=fake, weak=weak, mismatch=mismatch),
              io.open(JSON_OUT, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print('\n已写出 %s' % JSON_OUT)
sys.exit(1 if fake else 0)
