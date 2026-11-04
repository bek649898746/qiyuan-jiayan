#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""gen_state_map —— C1 状态普查（D4c 地基重做 · 2026-11-03 虾米）。

修前（审计递单 `_施工单_state_map地基三缺陷_启元_20261103.md`）三缺陷：
  ① 函数前向声明 94 条落 `decl@行号`（`名(...);` 没有 `{`）；
  ② 模块归属看「前 60 行窗口」+ M0 先匹配 ⟹ M0 吃掉 35.8%（`参数` 高频词）；
  ③ 一行多声明 / 多维数组 静默丢失；判据按「行」覆盖，看不见同行的第 2..N 个声明。

本版按**声明**扫描（不是按行）：
  · 先屏蔽注释/字符串（保留换行与偏移），再找**独立 token** `静`；
  · 语句按顶层 `;` / `{` 解析：函数定义/前向声明/结构体/变量（含多声明、多维数组、函数指针）；
  · 每个声明 = 一条 item；解析不出 ⟹ **响亮失败**（不再写 `decl@行号`）；
  · 模块归属 = 最近外层函数名 + 声明本身（不再看 60 行窗口）；M0 放最后且只认
    `main/驱动/缓存/命令行/argv/argc`（`参数` 不再作为 M0 关键词）；
  · 写 `source_sha256` / `source_lines` / `scope` / `excluded`，让「改了源忘了重生成」当场红。

用法: python scripts/gen_state_map.py [--out state_map.json]
退出码: 0 = 成功；1 = 有解析不出的静声明（fail-closed）
"""
import argparse
import hashlib
import json
import os
import re
import sys
import time
from collections import Counter

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')

ID = r'[A-Za-z_\u4e00-\u9fff][\w\u4e00-\u9fff]*'
RULES = [
    ('M8', ('ELF', 'Linux', 'SysV')),
    ('M9', ('自检', '测试', '--test', '--graph', '--interface', '--dj', '--serve', '--refs', '转储', 'dump', '诊断')),
    ('M5', ('IR', '指令链', 'CFG', 'SSA', '基本块', 'def-use', '回填', '支配', 'DF')),
    ('M6', ('优化', '窥孔', 'pass', '折叠', '寄存器池')),
    ('M7', ('码生成', '后端', 'PE', 'COFF', '汇编', '发射', '目标文件', '寄存器')),
    ('M4', ('类型', '语义', '借用', '泛型', '接口', '所有权', '生命周期')),
    ('M3', ('解析', '语法', 'AST', '抽象语法', '语句', '声明')),
    ('M2', ('词法', '词元', 'token', '宏', '预处理', '#包含', '#定义')),
    # M0 最后；`参数` 不再入表（审计：60 行窗口里几乎必中，M0 吃掉 35.8%）
    ('M0', ('main', '驱动', '缓存', '--cache', '--repeat', '命令行', 'argv', 'argc')),
]


def mask_comments_strings(s):
    out = list(s)
    i = 0
    n = len(s)
    while i < n:
        if s.startswith('/*', i):
            j = s.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif s.startswith('//', i):
            j = s.find('\n', i + 2)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif s[i] in ('"', "'"):
            q = s[i]
            j = i + 1
            while j < n:
                if s[j] == '\\':
                    j += 2
                    continue
                if s[j] == q:
                    j += 1
                    break
                j += 1
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        else:
            i += 1
    return ''.join(out)


def split_top_commas(s):
    out, buf, dep = [], [], 0
    for ch in s:
        if ch in '([{':
            dep += 1
        elif ch in ')]}':
            dep -= 1
        if ch == ',' and dep == 0:
            out.append(''.join(buf))
            buf = []
        else:
            buf.append(ch)
    out.append(''.join(buf))
    return out


def strip_attributes(s):
    out = []
    i = 0
    n = len(s)
    while i < n:
        if s.startswith('__attribute__', i):
            j = s.find('(', i)
            if j >= 0:
                dep = 0
                k = j
                while k < n:
                    if s[k] == '(':
                        dep += 1
                    elif s[k] == ')':
                        dep -= 1
                        if dep == 0:
                            k += 1
                            break
                    k += 1
                i = k
                continue
        out.append(s[i])
        i += 1
    return ''.join(out)


def decl_name(part):
    part = part.strip()
    fp = re.search(r'\(\s*\*\s*(' + ID + r')\s*\)', part)
    if fp:
        return fp.group(1)
    # cut top-level initializer
    eq = None
    dep = 0
    for i, ch in enumerate(part):
        if ch in '([{':
            dep += 1
        elif ch in ')]}':
            dep -= 1
        elif ch == '=' and dep == 0:
            eq = i
            break
    head = part[:eq] if eq is not None else part
    # name = last identifier before first top-level [ or (
    cut = len(head)
    dep = 0
    for i, ch in enumerate(head):
        if ch in '[(':
            if dep == 0:
                cut = i
                break
            dep += 1
        elif ch in ')]':
            dep -= 1
    ids = re.findall(ID, head[:cut])
    return ids[-1] if ids else None


def func_name(head):
    h = strip_attributes(head)
    cut = h.find('(')
    pre = h[:cut] if cut >= 0 else h
    ids = re.findall(ID, pre)
    return ids[-1] if ids else None


def pick_module(ctx):
    for mod, keys in RULES:
        for k in keys:
            if mod == 'M0' and k == 'main':
                if re.search(r'\bmain\b', ctx):
                    return mod, k
            elif k in ctx:
                return mod, k
    return 'M1', 'default'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='state_map.json')
    a = ap.parse_args()
    text = open(SRC, encoding='utf-8').read()
    n = len(text)
    masked = mask_comments_strings(text)
    lines = text.splitlines()

    items = []
    funcs = []  # (start_pos, brace_pos, end_pos, name)
    unparsed = []

    def line_of(pos):
        return text.count('\n', 0, pos) + 1

    def skip_ws(i):
        while i < n and masked[i].isspace():
            i += 1
        return i

    def match_brace(pos):
        dep = 1
        j = pos + 1
        while j < n and dep:
            if masked[j] == '{':
                dep += 1
            elif masked[j] == '}':
                dep -= 1
            j += 1
        return j - 1

    pos = 0
    while True:
        mm = re.search(r'(?<![\w\u4e00-\u9fff])静(?=\s)', masked[pos:])
        if not mm:
            break
        start = pos + mm.start()
        ln = line_of(start)
        i = skip_ws(start + 1)
        raw_line = lines[ln - 1] if 0 <= ln - 1 < len(lines) else ''
        indent = len(raw_line) - len(raw_line.lstrip())

        # ---- 静态结构体：静 构 { ... } 名;（字段不单独计数）----
        # 注意：`静 构 <类型> *变量` 是**变量声明**，不是结构体定义（审计 L4052 陷阱）。
        if re.match(r'构\s*(?:' + ID + r'\s*)?\{', masked[i:i + 300]):
            b = masked.find('{', i)
            if b < 0:
                unparsed.append((ln, text[start:start + 80]))
                pos = start + 1
                continue
            e = match_brace(b)
            semi = masked.find(';', e)
            semi = n if semi < 0 else semi
            name_m = re.match(r'\s*(' + ID + r')', masked[e + 1:min(semi, e + 200)])
            name = name_m.group(1) if name_m else 'struct@%d' % ln
            items.append(dict(name=name, line=ln, indent=indent, array=True,
                              kind='struct', _pos=start, _text=masked[i:b].strip()))
            pos = semi + 1
            continue

        # ---- 扫到顶层 ';' 或 '{' ----
        dep_p = 0
        j = i
        term = None
        while j < n:
            ch = masked[j]
            if ch in '([':
                dep_p += 1
            elif ch in ')]':
                dep_p -= 1
            elif ch == '{' and dep_p == 0:
                term = '{'
                break
            elif ch == ';' and dep_p == 0:
                term = ';'
                break
            j += 1
        if term is None:
            unparsed.append((ln, text[start:start + 80]))
            pos = start + 1
            continue
        head = masked[i:j]

        if term == '{':
            if '(' in head:
                name = func_name(head)
                if not name:
                    unparsed.append((ln, text[start:start + 100]))
                    pos = j + 1
                    continue
                end = match_brace(j)
                funcs.append((start, j, end, name))
                items.append(dict(name=name, line=ln, indent=indent, array=False,
                                  kind='static-function', _pos=start, _text=head.strip()))
                pos = j + 1
                continue
            # 变量 + 大括号初始化
            e = match_brace(j)
            semi = masked.find(';', e)
            semi = n if semi < 0 else semi
            decl = masked[i:semi]
            for part in split_top_commas(decl):
                nm = decl_name(part)
                if nm:
                    items.append(dict(name=nm, line=ln, indent=indent, array='[' in part,
                                      kind='variable', _pos=start, _text=decl.strip()))
                else:
                    unparsed.append((ln, text[start:start + 100]))
            pos = semi + 1
            continue

        # ---- term == ';'：函数前向声明 or 变量 ----
        if '(' in head:
            name = func_name(head)
            if not name:
                unparsed.append((ln, text[start:start + 100]))
            else:
                items.append(dict(name=name, line=ln, indent=indent, array=False,
                                  kind='static-proto', _pos=start, _text=head.strip()))
        else:
            for part in split_top_commas(head):
                nm = decl_name(part)
                if nm:
                    items.append(dict(name=nm, line=ln, indent=indent, array='[' in part,
                                      kind='variable', _pos=start, _text=head.strip()))
                else:
                    unparsed.append((ln, text[start:start + 100]))
        pos = j + 1

    if unparsed:
        print('[RED] gen_state_map: %d 条静声明无法解析（fail-closed）：' % len(unparsed))
        for ln, snip in unparsed[:10]:
            print('  L%d %r' % (ln, snip))
        return 1

    # 最近外层函数（严格包含；函数头自身不算）
    def enclosing_of(p):
        best = None
        for fs, fb, fe, fn in funcs:
            if fs < p < fe:
                if best is None or fs > best[0]:
                    best = (fs, fb, fe, fn)
        return best

    by_mod = Counter({('M%d' % i): 0 for i in range(10)})
    by_kind = Counter()
    out_items = []
    for it in items:
        enc = enclosing_of(it['_pos'])
        if enc:
            fs, fb, fe, fn = enc
            ctx = fn + ' ' + masked[fb + 1:min(fe, fb + 6000)] + ' ' + it['_text']
            enc_name = fn
        else:
            ln = it['line']
            ctx = '\n'.join(lines[max(0, ln - 16):ln + 5]) + ' ' + it['_text']
            enc_name = ''
        mod, rule = pick_module(ctx)
        by_mod[mod] += 1
        by_kind[it['kind']] += 1
        out_items.append({
            'name': it['name'], 'line': it['line'], 'indent': it['indent'],
            'array': it['array'], 'module': mod, 'rule': rule,
            'kind': it['kind'], 'enclosing': enc_name,
        })

    out = {
        'generated_at': time.strftime('%Y-%m-%d %H:%M:%S'),
        'source': 'srclib_jiayan/qcc_zh.jy',
        'source_sha256': hashlib.sha256(open(SRC, 'rb').read()).hexdigest(),
        'source_lines': len(lines),
        'scope': '文件级/函数级 static 声明；结构体字段与其内部声明不单独计数；非 static 局部变量不计',
        'excluded': {'struct_fields': True, 'local_vars': True},
        'total': len(out_items),
        'assigned': len(out_items),
        'unassigned': 0,
        'unparsed': 0,
        'default_m1': sum(1 for it in out_items if it['rule'] == 'default'),
        'by_kind': dict(sorted(by_kind.items())),
        'by_module': dict(sorted(by_mod.items())),
        'items': out_items,
    }
    path = os.path.join(ROOT, a.out)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(out, f, ensure_ascii=False, indent=2)
        f.write('\n')
    print('state_map: total=%d unparsed=0 default_m1=%d by_kind=%s by_module=%s' % (
        out['total'], out['default_m1'], out['by_kind'], out['by_module']))
    print('written', path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
