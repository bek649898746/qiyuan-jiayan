#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_elf_cn_entry —— ORD-20261107-137 判据（ELF 后端的**中文入口 `主`**）。

缺陷（2026-11-07 审计递单）：
  `整 主(空){ 返 0; }` 用 `--target=elf` 编出来 rc=0，但一跑 **SIGILL（exit 132）**；
  把入口改成 `main` 就正常。而 `manual/中文关键字表.md` 明写「`主` ≡ `main`」。

根因（2026-11-07 虾米）：
  ELF 发射器只查 `函数起查("main")`，没有 PE 路径那条 `main → 主` 回退 ⟹ 入口落到兜底偏移。

判据（正例必须真跑出正确退出码，缺 WSL 即 FAIL，不许空过）：
  A `整 主(空){ 返 0; }`                    → 运行 rc=0
  B `整 加数(…)` + `整 主(空){ 返 加数(3,4); }` → 运行 rc=7
  C `整 main(空){ 返 0; }`（对照组）         → 运行 rc=0
  D PE 目标下 `主` 仍可编（跨目标不回归）    → rc=0

用法: python scripts/check_elf_cn_entry.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全过；1 = 任一红（含 WSL 不可用）
"""
import argparse
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CASES = [
    ('A 主 空返回 0', '整 主(空){ 返 0; }\n', 0),
    ('B 主 调中文子函数 3+4', '整 加数(整 甲, 整 乙){ 返 甲+乙; }\n'
                              '整 主(空){ 返 加数(3,4); }\n', 7),
    ('C main 对照', '整 main(空){ 返 0; }\n', 0),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', **kw)


def wsl_bash(script):
    """用 `wsl -e bash -c`（**不要**用 `wsl -d Ubuntu -- bash -c`）。

    实测（2026-11-07）：后者会把 `$?` 在外层先展开一次 ⟹ 脚本里的 `echo __RC=$?` 拿到的是
    **外层命令**的退出码 0，而不是被测程序的退出码 —— 判据会静默把「跑出 0」当成事实。
    同一命令、同一二进制：`-d Ubuntu -- bash -c` 报 0，`-e bash -c` 报 7（真值 7）。
    """
    return run(['wsl', '-e', 'bash', '-c', script])


def wsl_path(p):
    p = os.path.abspath(p).replace('\\', '/')
    if len(p) > 1 and p[1] == ':':
        return '/mnt/' + p[0].lower() + p[2:]
    return p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    bad = []

    if shutil.which('wsl') is None:
        print('[FAIL] 缺 wsl.exe —— 无法真跑（fail-closed，不空过）')
        return 1
    probe = wsl_bash('uname -s')
    if probe.returncode != 0 or 'Linux' not in (probe.stdout or ''):
        print('[FAIL] WSL 不可用 (rc=%d, out=%r)' % (probe.returncode, (probe.stdout or '')[:120]))
        return 1

    work = tmp_dir('elfcnentry')
    try:
        for idx, (label, src_text, want) in enumerate(CASES):
            src = os.path.join(work, 'c%d.c' % idx)
            elf = os.path.join(work, 'c%d.elf' % idx)
            with open(src, 'w', encoding='utf-8', newline='\n') as f:
                f.write(src_text)
            r = run([cc, src, '--target=elf', '-o', elf], cwd=ROOT)
            if r.returncode != 0 or not os.path.exists(elf):
                print('[FAIL] %s 编译 rc=%d  %s' % (label, r.returncode,
                                                    (r.stdout or r.stderr or '')[:160]))
                bad.append(label)
                continue
            q = wsl_bash('chmod +x %s && %s; echo __RC=$?' % (wsl_path(elf), wsl_path(elf)))
            out = (q.stdout or '') + (q.stderr or '')
            rc = -1
            for line in out.splitlines():
                if line.startswith('__RC='):
                    rc = int(line[5:].strip())
            ok = rc == want
            print('[%s] %s 运行 rc=%d（期望 %d）' % ('PASS' if ok else 'FAIL', label, rc, want))
            if not ok:
                bad.append(label)
                print('       输出: %r' % out[-200:])

        # D：PE 目标下 主 仍可编（跨目标不回归）
        src = os.path.join(work, 'p.c')
        with open(src, 'w', encoding='utf-8', newline='\n') as f:
            f.write('整 主(空){ 返 0; }\n')
        r = run([cc, src, '-o', os.path.join(work, 'p.exe')], cwd=ROOT)
        ok = r.returncode == 0
        print('[%s] D PE 目标 主 可编 rc=%d' % ('PASS' if ok else 'FAIL', r.returncode))
        if not ok:
            bad.append('D PE')
    finally:
        shutil.rmtree(work, ignore_errors=True)

    if bad:
        print('=== ORD-20261107-137 判据: FAIL -> %s ===' % bad)
        return 1
    print('=== ORD-20261107-137 判据: PASS（ELF 中文入口 `主` 真跑出正确退出码）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
