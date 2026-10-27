# -*- coding: utf-8 -*-
"""check_unknown_type_census —— G-D48-PRE「未知类型名」普查（编译器探针，非纯 grep）。

方法：
  ① 对每个语料 .c 跑 `--interface`，取编译器**自己**登记的 typedefs/structs 为「已知类型」；
  ② 从源行首抽取声明形态 `T name;` / `T name =` / `T name[` 的 T；
  ③ 已知 = 内建类型 + interface.typedefs + interface.structs + 本文件 typedef；
  ④ 剩余 T = 编译器**静默接受**的候选未知类型名（`-c` 下 `nosuchtype x;` 实测 rc=0）。

⚠️ 这是**近似普查**：行首抽取会漏掉多行声明/宏类型；输出是「候选清单 + 出处计数」，
   用于决定 G-D48-COBJDIAG 的 (a)/(b)/(c)，不是判据红绿。

用法: python scripts/check_unknown_type_census.py [--corpus DIR] [--limit N] [--out LOG]
退出: 0 = 普查完成（不判红绿）；2 = 语料/编译器缺失
"""
import argparse
import glob
import io
import json
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILTIN = set('''void char short int long float double signed unsigned _Bool bool size_t ssize_t
整 字 短 长 长 长 无 有 单 双 空 构 枚 联 合 静 外 常 易失 无符号 有符号'''.split())
KEYWORDS = set('''if else for while do switch case default return break continue goto sizeof
typedef struct union enum static extern const volatile inline register auto'''.split())


def iface(cc, src):
    r = subprocess.run([cc, '--interface', src], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=60)
    t = (r.stdout or '') + (r.stderr or '')
    i = t.find('{"interface"')
    if i < 0:
        return None
    try:
        return json.loads(t[i:]).get('interface', {})
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    ap.add_argument('--corpus', default=r'D:\qiyuan_home\corpus\lua-5.4.7')
    ap.add_argument('--limit', type=int, default=60)
    ap.add_argument('--out', default=None)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    files = sorted(glob.glob(os.path.join(a.corpus, '*.c')))[:a.limit]
    if not files:
        print('[FAIL] 语料为空: %s' % a.corpus)
        return 2
    counted = Counter()
    where = defaultdict(set)
    parsed = 0
    for p in files:
        it = iface(cc, p)
        if it is None:
            continue
        parsed += 1
        known = set(BUILTIN) | set(KEYWORDS)
        for t in it.get('typedefs', []) or []:
            known.add(t.get('name', ''))
        for s in it.get('structs', []) or []:
            known.add(s.get('name', ''))
        txt = open(p, encoding='utf-8', errors='replace').read()
        for m in re.finditer(r'typedef\s+[^;]*?\b([A-Za-z_]\w*)\s*;', txt):
            known.add(m.group(1))
        for line in txt.splitlines():
            m = re.match(r'\s*(?:静|外|常)?\s*([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*(?:[;=,\[]|\))', line)
            if not m:
                continue
            t = m.group(1)
            if t in known or t in KEYWORDS:
                continue
            counted[t] += 1
            where[t].add(os.path.basename(p))
    L = ['# G-D48-PRE 未知类型名普查（近似）',
         '# 语料: %s（%d 文件，%d 个 --interface 解析成功）' % (a.corpus, len(files), parsed),
         '# 方法: --interface 已知 typedefs/structs + 本文件 typedef + 内建；行首声明抽取',
         '', '| 候选类型名 | 出现次数 | 出处（前 3） |', '|:--|--:|:--|']
    for name, n in counted.most_common():
        L.append('| `%s` | %d | %s |' % (name, n, ', '.join(sorted(where[name])[:3])))
    if not counted:
        L.append('| （无） | 0 | — |')
    out = '\n'.join(L) + '\n'
    print(out)
    if a.out:
        open(a.out, 'w', encoding='utf-8', newline='\n').write(out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
