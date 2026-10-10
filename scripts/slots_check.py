# -*- coding: utf-8 -*-
"""slots_check —— 「静默兜底 8 槽」单文件检查器（**权威口径**，与 silent_matrix 逐字一致）。

为什么需要它（R633 三条量具教训）:
  开发中要快速看「某个文件触发了哪些槽」，此前用临时脚本 `cslots.py`，它带 `-D __GNUC__=5`
  —— 那是**非常规参数、会改变行为**（同一探针在该参数下报「其它静默=2」，权威参数下只有 {1:1}）
  ⟹ 得出的「有/无静默」结论是**假的**。
  另一个坑: 用 PowerShell `*>` 重定向会令中文**彻底乱码**（按 GBK 解码 UTF-8 再存 UTF-16），
  于是 `'静默' in line` 之类统计**永远失配**，此前数次「零静默」同样是假结论。
  ⟹ 本器: (a) 参数与 `silent_matrix.compile_one` **逐字一致**; (b) 用 python 直接 capture（不经 shell 中转）。

用法:
  python scripts/slots_check.py <文件> [更多文件...]           # 默认编译器 qcc_zh_v1.exe
  python scripts/slots_check.py --cc <编译器> <文件...>
  python scripts/slots_check.py --corpus lua lfunc.c           # 语料内文件（自动补 lua 的 -D/-I）

输出: 每文件一行 `槽N=次数`（取自编译器尾部的 `[静默兜底 汇总]`，与 silent_matrix 同源），
      以及逐次告警名。**零静默** 显示为 `{}`。
退出码: 恒 0（本器是量具，不作判红；判红由闸⑦ 之类专职检查器承担）。
"""
import argparse
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPUS_ROOT = r'D:\qiyuan_home\corpus'
TARGETS = {                      # 与 silent_matrix.TARGETS 逐字一致
    'lua':   dict(dir='lua-5.4.7',      defs=['-D', 'LUA_USE_WINDOWS']),
    'zlib':  dict(dir='zlib-1.3.1',     defs=[]),
    'libpng': dict(dir='libpng-1.6.43', defs=[]),
}
SUMM = re.compile(r'槽(\d):\s*(.+?)\s*x(\d+)')


def check(cc, src, defs):
    obj = os.path.join(os.environ.get('TEMP', '.'), '_slots_check.obj')
    env = dict(os.environ, QCC_DBG_SILENT='1')
    cmd = [cc, '--lenient'] + defs + ['-I', os.path.dirname(src), '-c', src, '-o', obj]
    r = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='replace',
                       env=env, timeout=600, cwd=REPO)
    txt = (r.stdout or '') + (r.stderr or '')
    slots = {int(m.group(1)): int(m.group(3)) for m in SUMM.finditer(txt)}
    names = {int(m.group(1)): m.group(2) for m in SUMM.finditer(txt)}
    per_hit = [l.strip() for l in txt.splitlines() if '[静默兜底]' in l and '汇总' not in l]
    return r.returncode, slots, names, per_hit


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--corpus', default='', choices=['', 'lua', 'zlib', 'libpng'])
    ap.add_argument('files', nargs='+')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    defs = TARGETS[a.corpus]['defs'] if a.corpus else []
    base = os.path.join(CORPUS_ROOT, TARGETS[a.corpus]['dir']) if a.corpus else ''
    for f in a.files:
        src = os.path.abspath(os.path.join(base, f) if base else f)
        rc, slots, names, hits = check(cc, src, defs)
        print('%-46s rc=%d slots=%s' % (os.path.basename(f), rc, slots or '{}'))
        for k in sorted(slots):
            print('      槽%d x%-3d %s' % (k, slots[k], names.get(k, '')))
        for h in hits[:8]:
            print('      ', h[:140])
    return 0


if __name__ == '__main__':
    sys.exit(main())
