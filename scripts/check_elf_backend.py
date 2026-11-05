# -*- coding: utf-8 -*-
"""ELF 后端端到端判据（ELF-E1/E2）—— fail-closed。

判据面（每条都必须由**真实产物**驱动，缺环境即 FAIL，不许空过）：
  1 编译：`qcc_zh_v1.exe --target=elf <probe.c> -o <probe.elf>` rc=0
  2 容器：`readelf -h` 认 `ELF64` / `X86-64` / `EXEC`
  3 真跑：WSL Ubuntu 里执行，stdout 逐字符匹配、退出码 == 42
  4 换心：`objdump -d` 中 `call *0x` == 0（无 IAT 间接调用）、真 `%gs:` 指令 == 0、`syscall` > 0

用法: python scripts/check_elf_backend.py [--cc qcc_zh_v1.exe] [--keep]
退出: 0 = 全过; 1 = 任一红（含 WSL 不可用）
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from qcc_tmp import tmp_dir

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROBE = r'''#include <stdio.h>
int main(int argc, char **argv) {
    printf("argc=%d\n", argc);
    if (argc > 1) printf("argv1=%s\n", argv[1]);
    return 42;
}
'''


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', **kw)


def wsl_bash(script):
    return run(['wsl', '-d', 'Ubuntu', '--', 'bash', '-c', script])


def wsl_path(win_path):
    p = os.path.abspath(win_path).replace('\\', '/')
    if len(p) > 1 and p[1] == ':':
        return '/mnt/' + p[0].lower() + p[2:]
    return p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    fails = []

    if shutil.which('wsl') is None:
        print('[FAIL] 缺 wsl.exe —— 无法真跑（fail-closed，不空过）')
        return 1
    probe = wsl_bash('uname -s')
    if probe.returncode != 0 or 'Linux' not in (probe.stdout or ''):
        print('[FAIL] WSL 不可用 (rc=%d, out=%r)' % (probe.returncode, (probe.stdout or '')[:120]))
        return 1

    # ⚠️ 不要用系统 TEMP：`C:\Users\ADMINI~1\...` 的 8.3 短名在 WSL 里不存在（Linux 看到 Administrator）
    #    ⟹ chmod/执行会静默失败。工作目录放 D 盘，路径无短名。
    base = os.environ.get('QCC_ELF_WORK')
    if base:
        os.environ.setdefault('QCC_TMP_ROOT', base)  # R896 轮：统一走 qcc_tmp 落点策略
    work = tmp_dir('elfchk')
    src = os.path.join(work, 'probe.c')
    elf = os.path.join(work, 'probe.elf')
    with open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(PROBE)

    env = dict(os.environ, QCC_SMALL_HEAP='1')
    r = run([cc, '--target=elf', src, '-o', elf], cwd=ROOT, env=env)
    if r.returncode != 0 or not os.path.exists(elf):
        print('[FAIL] 编译失败 rc=%d: %s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[:300]))
        return 1

    w = wsl_path(elf)
    h = wsl_bash("readelf -h '%s'" % w)
    hout = h.stdout or ''
    for need in ('ELF64', 'X86-64', 'EXEC'):
        if need not in hout:
            fails.append('readelf -h 缺 %s' % need)

    # 用脚本文件驱动（不走 `bash -c "…$?…"`：wsl.exe 的命令行重组会吃掉 `$?`，实测恒得 0）
    wdir = os.path.dirname(w)
    rsh = os.path.join(work, 'run.sh')
    with open(rsh, 'w', encoding='utf-8', newline='\n') as f:
        f.write('#!/bin/bash\ncd %s\nchmod +x probe.elf\n./probe.elf alpha beta\nrc=$?\necho "__RC=$rc"\n' % wdir)
    runsh = wsl_bash("bash '%s'" % wsl_path(rsh))
    rout = runsh.stdout or ''
    m = re.search(r'__RC=(-?\d+)', rout)
    rc = int(m.group(1)) if m else None
    body = rout.split('__RC=')[0]
    if rc != 42:
        fails.append('退出码 %r != 42' % rc)
    if 'argc=3' not in body:
        fails.append('stdout 缺 argc=3 (实际=%r)' % body[:120])
    if 'argv1=alpha' not in body:
        fails.append('stdout 缺 argv1=alpha (实际=%r)' % body[:120])

    d = wsl_bash("objdump -d '%s'" % w)
    dis = d.stdout or ''
    n_ind = len(re.findall(r'call\s+\*0x', dis))
    if n_ind != 0:
        fails.append('IAT 间接调用 %d 条 (应 0)' % n_ind)
    # 只统计「写 TEB」形态 `mov %rXX,%gs:0x..` —— 真指令全是这一形；数据区误反汇编是
    # `and %ah,%gs:0x65(%rdi)` / `mov %ss,...` 之类，不含 reg→%gs 的 mov
    real_gs = [ln for ln in dis.splitlines() if re.search(r'mov\s+%\w+,%gs:', ln)]
    if real_gs:
        fails.append('真 %gs 指令 %d 条 (应 0): %s' % (len(real_gs), real_gs[:2]))
    n_sys = len(re.findall(r'\bsyscall\b', dis))
    if n_sys <= 0:
        fails.append('syscall 0 条 (应 > 0)')

    print('== ELF 后端判据 ==')
    print('  编译器 : %s' % os.path.basename(cc))
    print('  产物   : %s (%d bytes)' % (elf, os.path.getsize(elf)))
    print('  真跑   : rc=%r stdout=%r' % (rc, body.strip().splitlines()[:2]))
    print('  call*0x=%d  real%%gs=%d  syscall=%d' % (n_ind, len(real_gs), n_sys))
    if fails:
        print('[FAIL] ' + '; '.join(fails))
        if not a.keep:
            shutil.rmtree(work, ignore_errors=True)
        return 1
    print('[PASS] ELF 后端：容器合法 + 真跑 42 + 无 IAT/无 TEB + 有 syscall')
    if a.keep:
        print('  保留: %s' % work)
    else:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
