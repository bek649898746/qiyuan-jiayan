#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""publish_round_evidence —— 轮次证据的**唯一写者**。

为什么需要它
------------
同一份事实曾经有 N 份手工副本：`_evidence/<round>/xxx.log` 一份、plan 点名的
`{SCRATCH}/xxx.log` 一份，各自在不同时刻被手工重定向写成，于是「审计读到哪一份」
决定了它看到的是新是旧 —— 上一轮就因此被打回（`{SCRATCH}/failopen_negative.log`
还是 706 B 的旧版，而 `_evidence` 里已经是 1287 B 的新版）。

本脚本把这条路封死：
  1. 每条验收步骤**只跑一次**，原始输出先写规范源 `_evidence/<round>/<name>.log`；
  2. 再**字节复制**到 `{SCRATCH}/<name>.log`（scratch 根由 `--scratch` 或
     `GOAL_SCRATCH` / `QCC_SCRATCH` 给出）；
  3. 每份日志首行统一写 `# cc=<build_zh.ps1 的 $expected> round=<round> cmd=<...> ts=<iso>`；
  4. 最后写 `<round>/MANIFEST.sha256`（路径 → sha256），由
     `check_report_consistency.py` 的 `replica_errors()` 逐条验副本一致。

此后**禁止手抄/手拷日志**：要更新证据，重跑本脚本。

用法
----
    python scripts/publish_round_evidence.py                # 全跑（约 25 分钟）
    python scripts/publish_round_evidence.py --only d6_35s  # 只跑某几步
    python scripts/publish_round_evidence.py --manifest-only  # 只重建 MANIFEST + 副本
"""
import argparse
import datetime
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

CC = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.environ.get('QCC_LEDGER_REPO') or os.path.join(os.path.dirname(CC), 'grok-build')
ROUND = '20261106_ledger_closure'
CANON = os.path.join(CC, '_evidence', ROUND)

# 这些步骤按设计就是「红的复现器」：`publish_refs` 的红 = 镜像还没部署（台账 ORD-20261107-134）。
# 它们的 rc≠0 是**预期**，不是本轮失败。
EXPECT_RED = {'publish_refs'}


def fixed_point():
    p = os.path.join(CC, 'scripts', 'build_zh.ps1')
    txt = io.open(p, 'rb').read().decode('utf-8', 'surrogateescape')
    m = re.search(r"^\$expected\s*=\s*'([0-9A-Fa-f]{8})'", txt, re.M)
    return m.group(1).upper() if m else '????????'


def newest_ledger_evidence():
    """`run_criteria` 的 crit 日志要落在台账仓最新证据目录的 logs/ 下（check_tasklist ⑧ 读这里）。"""
    base = os.path.join(LEDGER, '_evidence')
    if not os.path.isdir(base):
        return os.path.join(LEDGER, '_evidence', 'unknown', 'logs')
    ds = [d for d in os.listdir(base) if d.startswith('2026') and
          os.path.isdir(os.path.join(base, d))]
    return os.path.join(base, max(ds), 'logs') if ds else os.path.join(base, 'unknown', 'logs')


def _steps():
    py = sys.executable
    crit = newest_ledger_evidence()
    # 顺序有依赖：判据先跑（刷新 crit 日志）→ tasklist 才可能新鲜；
    # report_consistency 恒在最后（它要读 MANIFEST + 副本）。
    return [
        ('criteria_batch', LEDGER, [py, 'scripts/run_criteria.py',
                                    '--only', 'ORD-20261105-126', 'ORD-20261106-129',
                                    'ORD-20261104-105', 'ORD-20261104-120',
                                    '--parallel', '1', '--timeout', '1800',
                                    '--scratch', crit]),
        ('failopen_negative', CC, [py, 'scripts/check_failopen_negatives.py', '--cc', 'qcc_zh_v1.exe']),
        ('publish_refs', CC, [py, 'scripts/check_publish_refs.py']),
        ('d6_35s', CC, [py, 'scripts/check_d6_35s.py']),
        ('regress', CC, ['__REGRESS__']),
        ('tasklist', LEDGER, [py, 'scripts/check_tasklist.py']),
        ('report_consistency', CC, [py, 'scripts/check_report_consistency.py', '--self-test']),
    ]


# 进 MANIFEST / 副本 的证据日志（`report_consistency` 是闸本身，排除以避免自指）
MANIFEST_NAMES = ('tasklist', 'failopen_negative', 'publish_refs',
                  'criteria_batch', 'd6_35s', 'regress')


REGRESS_SUBSTEPS = [
    ('1) 自举 v1==v2', ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                        '-File', os.path.join(CC, 'scripts', 'build_zh.ps1'), '-NoSeed']),
    ('2) check_elf_selfhost', [sys.executable, 'scripts/check_elf_selfhost.py']),
    ('3) check_elf_backend', [sys.executable, 'scripts/check_elf_backend.py']),
    ('4) check_sysv_abi', [sys.executable, 'scripts/check_sysv_abi.py']),
    ('5) check_elf_c_unit', [sys.executable, 'scripts/check_elf_c_unit.py']),
    ('6) check_elf_lua_run', [sys.executable, 'scripts/check_elf_lua_run.py']),
]


def run_regress(env):
    """plan 步骤 5：六条依次跑，返回 (文本, 是否全绿)。"""
    buf, oks = [], []
    for label, cmd in REGRESS_SUBSTEPS:
        t0 = datetime.datetime.now()
        p = subprocess.run(cmd, cwd=CC, capture_output=True, env=env)
        dt = (datetime.datetime.now() - t0).total_seconds()
        buf.append('=== %s ===' % label)
        buf.append('$ ' + ' '.join(cmd))
        buf.append((p.stdout + p.stderr).decode('utf-8', 'replace').rstrip())
        buf.append('')
        buf.append('[rc=%d elapsed=%.1fs]' % (p.returncode, dt))
        buf.append('')
        oks.append((label, p.returncode, dt))
        print('  %s rc=%d %.1fs' % (label, p.returncode, dt))
    buf.append('=== SUMMARY ===')
    for label, rc, dt in oks:
        buf.append('%s rc=%d %.1fs' % (label, rc, dt))
    ok = all(rc == 0 for _, rc, _ in oks)
    buf.append('RESULT: %s' % ('PASS' if ok else 'FAIL'))
    return '\n'.join(buf) + '\n', ok


def scratch_roots(given):
    """副本要写去哪：显式给出的 + 环境变量 + **同一父目录下的全部兄弟目录**。

    为什么收兄弟目录：审计侧与实现侧拿到的 `{SCRATCH}` 不是同一个路径
    （实现侧 `…/implementer`、审计侧 `…/skeptic-0`），只写一处 ⟹ 审计读到空目录。
    兄弟目录都是临时证据区，逐字节复制一份成本可忽略。
    """
    roots = []
    for r in (given or []):
        if r:
            roots.append(os.path.abspath(r))
    for k in ('GOAL_SCRATCH', 'QCC_SCRATCH'):
        v = os.environ.get(k)
        if v:
            roots.append(os.path.abspath(v))
    for r in list(roots):
        parent = os.path.dirname(r)
        if parent and os.path.isdir(parent):
            for d in sorted(os.listdir(parent)):
                p = os.path.join(parent, d)
                if os.path.isdir(p):
                    roots.append(os.path.abspath(p))
    out = []
    for r in roots:
        if r not in out:
            out.append(r)
    return out


def write_with_header(path, body, cmd):
    head = '# cc=%s round=%s cmd=%s ts=%s\n' % (
        fixed_point(), ROUND, cmd, datetime.datetime.now().isoformat(timespec='seconds'))
    with io.open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(head + body)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def write_manifest(names, scratch_dirs):
    lines = ['# MANIFEST.sha256 · 由 scripts/publish_round_evidence.py 生成 · 勿手改',
             '# <sha256>  <规范化路径（相对 qiyuan-jiayan）>',
             '# 副本 = 每个 {SCRATCH} 根下的同名文件；两份必须逐字节相同。']
    for n in names:
        p = os.path.join(CANON, n + '.log')
        rel = os.path.relpath(p, CC).replace('\\', '/')
        lines.append('%s  %s' % (sha256_of(p), rel))
        for d in scratch_dirs:
            dst = os.path.join(d, n + '.log')
            os.makedirs(d, exist_ok=True)
            shutil.copyfile(p, dst)
    out = os.path.join(CANON, 'MANIFEST.sha256')
    with io.open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print('[MANIFEST] %s（%d 条；副本写入 %d 个 scratch 根%s）'
          % (out, len(names), len(scratch_dirs), '' if scratch_dirs else '：未解析到 scratch'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', nargs='*', default=None)
    ap.add_argument('--scratch', action='append', default=None,
                    help='可重复；缺省读 GOAL_SCRATCH/QCC_SCRATCH，并自动带同父目录的兄弟目录')
    ap.add_argument('--manifest-only', action='store_true')
    a = ap.parse_args()

    os.makedirs(CANON, exist_ok=True)
    scratch_dirs = scratch_roots(a.scratch)
    env = dict(os.environ)
    if scratch_dirs:
        env['GOAL_SCRATCH'] = scratch_dirs[0]      # 让子判据也能校验副本
    steps = _steps()
    names = [s[0] for s in steps]
    run = [] if a.manifest_only else (a.only or names)
    bad = []

    def _one(name, cwd, cmd):
        print('[RUN] %s' % name)
        if cmd == ['__REGRESS__']:
            body, ok = run_regress(env)
            write_with_header(os.path.join(CANON, name + '.log'), body, 'regress(6 steps)')
        else:
            p = subprocess.run(cmd, cwd=cwd, capture_output=True, env=env)
            body = (p.stdout + p.stderr).decode('utf-8', 'replace')
            write_with_header(os.path.join(CANON, name + '.log'), body, ' '.join(cmd))
            ok = p.returncode == 0
            print('  rc=%d  %s' % (p.returncode, CANON))
        if not ok and name not in EXPECT_RED:
            bad.append(name)
        elif not ok:
            print('  （%s 预期为红：见台账 ORD-20261107-134）' % name)

    def _manifest():
        present = [n for n in MANIFEST_NAMES
                   if os.path.exists(os.path.join(CANON, n + '.log'))]
        write_manifest(present, scratch_dirs)
        return present

    # 第一段：除「闸自己」以外的全部步骤
    for name, cwd, cmd in steps:
        if name == 'report_consistency' or name not in run:
            continue
        _one(name, cwd, cmd)

    # 先落 MANIFEST + 副本，闸才有东西可验
    present = _manifest()

    # 第二段：闸自己（放最后，读的就是刚落好的 MANIFEST）
    if 'report_consistency' in run:
        for name, cwd, cmd in steps:
            if name == 'report_consistency':
                _one(name, cwd, cmd)

    if bad:
        print('=== publish_round_evidence: FAIL（%s）===' % ', '.join(bad))
        return 1
    print('=== publish_round_evidence: PASS（%d 份证据日志已同步到规范源与副本）===' % len(present))
    return 0


if __name__ == '__main__':
    sys.exit(main())
