# -*- coding: utf-8 -*-
"""check_unknown_type_census —— G-D48-PRE「未知类型名」普查（编译器探针，非纯 grep）。

方法：
  ① 对每个语料 .c 跑 `--interface`，取编译器**自己**登记的 typedefs/structs 为「已知类型」；
  ② 去掉注释/字符串后，从源行首抽取声明形态 `T name;` / `T name =` / `T name[` 的 T；
  ③ 已知 = 内建类型 + interface.typedefs + interface.structs + 本文件 typedef；
  ④ 剩余 T 交给**编译器探针**裁决：写 `<T> qcc_probe_slot;` 用 `-c` 编一次 ——
     rc=0 ⟹ 编译器**静默接受**了一个没定义的类型（真候选）；rc≠0 ⟹ 该词不是类型名（噪声，剔除）。

  2026-10-27 修正（skeptic）：旧版只做行首 grep ⟹ 注释里的英文词（conventional / throw /
  needed / integer）被当成类型名；现在「注释剥离 + 编译器探针」两道过滤，并同时给出被拒清单，
  让「候选 = 编译器真接受」这件事可复核。

⚠️ 仍是**近似普查**：行首抽取会漏掉多行声明/宏类型；输出是「候选清单 + 出处计数」，
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
import tempfile
from collections import Counter, defaultdict

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILTIN = set('''void char short int long float double signed unsigned _Bool bool size_t ssize_t
整 字 短 长 无 有 单 双 空 构 枚 联 合 静 外 常 易失 无符号 有符号'''.split())
KEYWORDS = set('''if else for while do switch case default return break continue goto sizeof
typedef struct union enum static extern const volatile inline register auto'''.split())


def strip_comments(txt, string_marker='\x01'):
    """把 // 与 /* */ 注释替换成空格、字符串/字符字面量替换成哨兵 \\x01（保留换行，行号不乱）。

    字符串用哨兵而非空格：`lua_pushliteral(L, LUA_DIRSEP "\\1" LUA_EXEC_DIR "\\n");`
    这类**跨字符串续行**在去掉引号后会残留 `   LUA_EXEC_DIR ... );`，看起来像声明 ——
    匹配前把行在首个哨兵处截断，即可剔除这种假候选（2026-10-27 skeptic：表里的噪声）。
    """
    out = []
    i, n = 0, len(txt)
    while i < n:
        c = txt[i]
        if c in '"\'':
            q = c
            out.append(string_marker)
            i += 1
            while i < n and txt[i] != q:
                if txt[i] == '\\':
                    i += 1
                if i < n and txt[i] == '\n':
                    out.append('\n')
                i += 1
            i += 1
        elif c == '/' and i + 1 < n and txt[i + 1] == '/':
            while i < n and txt[i] != '\n':
                i += 1
        elif c == '/' and i + 1 < n and txt[i + 1] == '*':
            i += 2
            while i + 1 < n and not (txt[i] == '*' and txt[i + 1] == '/'):
                if txt[i] == '\n':
                    out.append('\n')
                i += 1
            i += 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


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


def probe_type(cc, name, workdir):
    """`T slot;` 用 -c 编一次：rc=0 ⟹ 编译器静默接受该未知类型名。"""
    slug = re.sub(r'\W', '_', name) or 'anon'
    src = os.path.join(workdir, 'probe_%s.c' % slug)
    obj = os.path.join(workdir, 'probe_%s.o' % slug)
    io.open(src, 'w', encoding='utf-8', newline='\n').write('%s qcc_probe_slot;\n' % name)
    try:
        r = subprocess.run([cc, '-c', src, '-o', obj], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=60)
        return r.returncode
    except Exception:
        return -99


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
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 2
    counted = Counter()
    where = defaultdict(set)
    parsed = 0
    decl = re.compile(r'\s*(?:静|外|常)?\s*([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*(?:[;=,\[]|\))')
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
        raw = io.open(p, encoding='utf-8', errors='replace').read()
        txt = strip_comments(raw)
        for m in re.finditer(r'typedef\s+[^;]*?\b([A-Za-z_]\w*)\s*;', txt):
            known.add(m.group(1))
        kept = set()
        for line in txt.splitlines():
            line = line.split('\x01')[0]   # 串接续行/含字符串的行：只在首个字面量之前找声明
            m = decl.match(line)
            if not m:
                continue
            t = m.group(1)
            if t in known or t in KEYWORDS:
                continue
            kept.add(t)
            counted[t] += 1
            where[t].add(os.path.basename(p))

    workdir = tempfile.mkdtemp(prefix='qcc_utype_')
    accepted, rejected = {}, {}
    for name in counted:
        rc = probe_type(cc, name, workdir)
        (accepted if rc == 0 else rejected)[name] = rc

    L = ['# G-D48-PRE 未知类型名普查（近似 + 编译器探针裁决）',
         '# 语料: %s（%d 文件，%d 个 --interface 解析成功）' % (a.corpus, len(files), parsed),
         '# 方法: 注释/字符串剥离 → 行首声明抽取 → `T slot;` 用 -c 探针（rc=0 = 编译器静默接受）',
         '# 汇总: 行首候选 %d → 探针确认 %d / 探针拒绝 %d（噪声）'
         % (len(counted), len(accepted), len(rejected)),
         '# 噪声处置: 注释/字符串内容不参与抽取（旧版把注释里的 conventional / needed / integer 当类型名），'
         '跨字符串续行（如 lua_pushliteral 里的 LUA_EXEC_DIR）在首个字面量处截断',
         '', '| 候选类型名（探针 rc=0：编译器静默接受） | 出现次数 | 出处（前 3） |',
         '|:--|--:|:--|']
    for name, n in sorted(accepted.items(), key=lambda kv: (-counted[kv[0]], kv[0])):
        L.append('| `%s` | %d | %s |' % (name, counted[name], ', '.join(sorted(where[name])[:3])))
    if not accepted:
        L.append('| （无） | 0 | — |')
    L += ['', '## 探针拒绝（不是类型名，已剔除）', '']
    if rejected:
        L.append('| 词 | 探针 rc | 出处 |')
        L.append('|:--|--:|:--|')
        for name, rc in sorted(rejected.items()):
            L.append('| `%s` | %d | %s |' % (name, rc, ', '.join(sorted(where[name])[:3])))
    else:
        L.append('（无）')
    out = '\n'.join(L) + '\n'
    print(out)
    if a.out:
        io.open(a.out, 'w', encoding='utf-8', newline='\n').write(out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
