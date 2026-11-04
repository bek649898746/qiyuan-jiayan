#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""gen_unity —— C2 生成式 unity（零移动切分 + #line 映射）。

设计（先证机制，再搬迁代码）：
  · 块边界 = 主源**顶层** `静` 声明行（缩进 0）；块从该行延续到下一个顶层 `静` 声明前。
  · 每块的模块标签来自 `state_map.json` 的该行归属；块内其余代码（函数体等）随块走。
  · `build/qcc_unity.jy` = 按**原文顺序**拼接所有块，每块前插 `#line <原行号> "原文件"`.
  · `src/<M?>_<名>.jy` = 同一批块的**模块视图**（按原行号排序），供 review / 后续搬迁。
  · 因为拼接顺序与原文完全一致、未重排任何函数 ⟹ 产物应与单文件逐字节相同
    （`check_split_equiv.py` 断言）。

用法: python scripts/gen_unity.py
"""
import json
import os
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')
MAP = os.path.join(ROOT, 'state_map.json')
OUTDIR = os.path.join(ROOT, 'build')
SRCDIR = os.path.join(ROOT, 'src')
NAMES = {
    'M0': '00_驱动', 'M1': '01_基础状态', 'M2': '10_词法', 'M3': '20_语法',
    'M4': '30_语义', 'M5': '40_IR', 'M6': '50_优化', 'M7': '60_后端_PE',
    'M8': '70_后端_ELF', 'M9': '90_自检',
}


def main():
    lines = open(SRC, encoding='utf-8', errors='replace').read().splitlines()
    sm = json.load(open(MAP, encoding='utf-8'))
    line_mod = [None] * (len(lines) + 1)
    line_indent = [0] * (len(lines) + 1)
    for it in sm['items']:
        ln = it['line']
        if 1 <= ln <= len(lines) and line_mod[ln] is None:
            line_mod[ln] = it['module']
            line_indent[ln] = it.get('indent', 0)

    # 块首 = 顶层(缩进 0)且有模块标签的行
    heads = [i for i in range(1, len(lines) + 1) if line_mod[i] and line_indent[i] == 0]
    if not heads:
        print('[ERR] 没有块首（state_map 为空？）')
        return 1
    first = 1
    # 第一个块首之前的内容（文件头 #include/#define）归 M1
    blocks = []
    if heads[0] > 1:
        blocks.append({'start': 1, 'end': heads[0] - 1, 'module': 'M1'})
    for k, h in enumerate(heads):
        end = (heads[k + 1] - 1) if k + 1 < len(heads) else len(lines)
        blocks.append({'start': h, 'end': end, 'module': line_mod[h]})

    os.makedirs(OUTDIR, exist_ok=True)
    os.makedirs(SRCDIR, exist_ok=True)
    rel = 'srclib_jiayan/qcc_zh.jy'

    def text_of(b, with_line):
        body = '\n'.join(lines[b['start'] - 1:b['end']])
        if with_line:
            return '#line %d "%s"\n%s\n' % (b['start'], rel, body)
        return body + '\n'

    # 1) 先写模块视图（带块标记 + #line），供 review 与诊断行号对照
    by_mod = {}
    for b in blocks:
        by_mod.setdefault(b['module'], []).append(b)
    files = {}
    for mod in sorted(by_mod):
        name = '%s_%s.jy' % (mod, NAMES.get(mod, mod))
        path = os.path.join(SRCDIR, name)
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            for b in sorted(by_mod[mod], key=lambda x: x['start']):
                f.write('/*@BLOCK %d*/\n' % b['start'])
                f.write('#line %d "%s"\n' % (b['start'], rel))
                f.write('\n'.join(lines[b['start'] - 1:b['end']]))
                f.write('\n/*@END*/\n')
        files[mod] = os.path.relpath(path, ROOT).replace('\\', '/')

    manifest = {'source': rel, 'blocks': blocks, 'files': files, 'block_count': len(blocks)}
    with open(os.path.join(SRCDIR, 'manifest.json'), 'w', encoding='utf-8') as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)
        f.write('\n')

    # 2) 正式拼接**从模块文件读回**（证明模块文件是拼接源），按原行号排序；
    #    拼接时剥掉 `/*@BLOCK*/` 与 `#line`（二者只服务于 review/诊断）。
    text_by_mod = {m: open(os.path.join(ROOT, p), encoding='utf-8').read() for m, p in files.items()}

    def extract(mod, start):
        t = text_by_mod[mod]
        marker = '/*@BLOCK %d*/\n' % start
        i = t.index(marker) + len(marker)
        if t[i:].startswith('#line '):
            i = t.index('\n', i) + 1
        j = t.index('\n/*@END*/', i)
        return t[i:j]

    unity = ''.join(extract(b['module'], b['start']) + '\n' for b in blocks)
    with open(os.path.join(OUTDIR, 'qcc_unity.jy'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(unity)

    manifest['unity_lines'] = unity.count('\n')
    with open(os.path.join(SRCDIR, 'manifest.json'), 'w', encoding='utf-8') as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)
        f.write('\n')
    print('blocks=%d unity_lines=%d files=%s' % (len(blocks), manifest['unity_lines'], files))
    return 0


if __name__ == '__main__':
    sys.exit(main())
