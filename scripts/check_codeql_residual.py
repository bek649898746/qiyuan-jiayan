# -*- coding: utf-8 -*-
"""check_codeql_residual —— CodeQL 最后 2 条真告警 + CI 可诊断性 + CI 假声明 的永久闸。

施工单《CodeQL 残留 2 条与 CI 可诊断性》闭环件（2026-10-27）：
  ① scripts/feat_err.c  / ② scripts/feat_exc.c
     `while (msg[i] != 0 && i < 15)` 在判界前先读 msg[i] ⟹
     `cpp/offset-use-before-range-check`。判据：必须是 `i < 15 && msg[i] != 0`。
  ③ ci.yml 的 bootstrap / nightly_full 必须把失败原因打成 `::error::`（否则审计方拿不到原因）。
  ③-b ci.yml 不许再写「C 版原样留在 jiayan-v1.0-stable」的假声明
     （实测 tag 内只有封存版的 55.7%）。
"""
import argparse
import os
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = REPO


def read(p):
    return open(os.path.join(ROOT, p), encoding='utf-8', errors='replace').read()


def main():
    global ROOT
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=None, help='要检查的树（默认本仓；用于修前/修后对照）')
    a = ap.parse_args()
    ROOT = a.root or REPO
    fails = []
    for p in ('scripts/feat_err.c', 'scripts/feat_exc.c'):
        s = read(p)
        if 'while (i < 15 && msg[i] != 0)' not in s:
            fails.append('%s: 缺「先判界再读」的修后形态' % p)
        if 'while (msg[i] != 0 && i < 15)' in s:
            fails.append('%s: 仍保留「先读后判界」的修前形态' % p)

    ci = read('.github/workflows/ci.yml')
    for needle in ('::error::build_zh', '::error::run_tests full'):
        if needle not in ci:
            fails.append('ci.yml: 缺错误外化 %r' % needle)
    for line in ci.splitlines():
        if '原样留在' in line and not any(k in line for k in ('修正', '错的', '不实', '原文写', '原文说')):
            fails.append('ci.yml: 仍含未更正的假声明: %s' % line.strip()[:120])
    if '原样留在该标签' in ci:
        fails.append('ci.yml: 仍含假声明「原样留在该标签」')
    if "github.ref == 'refs/heads/publish'" not in ci:
        fails.append('ci.yml: bootstrap 未在 publish push 上触发（日常绿仍会掩盖自举）')

    # 扫描范围收窄（施工单《CodeQL 扫描范围收窄》）：3,142 → ~80 的五类排除必须在盘上
    cfg = read('.github/codeql/codeql-config.yml')
    for needle in ('srclib/qcc_x86.c', 'srclib/asm_zh.c', "'seed/**'", "'tests/**'",
                   'srclib/_dbg_*.c', 'srclib/qcc_x86_dbg.c', 'scripts/_*.c'):
        if needle not in cfg:
            fails.append('codeql-config.yml: 缺收窄排除 %r' % needle)
    if 'security-extended' not in cfg:
        fails.append('codeql-config.yml: queries 不是 security-extended（安全口径未收口）')

    if fails:
        print('[FAIL] CodeQL 残留 / CI 可诊断性: %d 项' % len(fails))
        for f in fails:
            print('   - ' + f)
        return 1
    print('[PASS] CodeQL 残留 / CI 可诊断性: 两处条件顺序 / 两处 ::error:: / 零假声明 / publish 自举')
    return 0


if __name__ == '__main__':
    sys.exit(main())
