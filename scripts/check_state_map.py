#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_state_map —— C1/D4c 状态普查判据（按**声明**比对，不按行）。

审计递单 `_施工单_state_map地基三缺陷_启元_20261103.md` 的核心批评：
  旧判据只断言「每个 `静` 行号被覆盖」⟹ 一行 4 个声明记 2 个也 PASS；
  函数前向声明 / 多维数组 / 注释后声明 静默丢失，判据看不见。

本判据：
  1. 表存在；`unparsed == 0`（任何 `decl@行号` 即红）；`total == len(items)`；
  2. `source_sha256` / `source_lines` 必须等于**当前**源码（改了源忘重生成 ⟹ 红）；
  3. 独立计数器（按行 + 顶层逗号，与生成器的 token/语句扫描不同路径）算 `N2`，
     `|total - N2| / N2 < 1%`；
  4. 逐行声明覆盖：独立计数 > 1 的行，items 条目数必须 ≥ 独立计数；
     所有 `名[N][M]` 多维数组名必须逐名出现在 items（同一行）；
  5. 模块占比：任一模块 > 30% ⟹ 红（审计：M0 35.8% 是垃圾桶）；
  6. 十模块声明齐全（M8 可 0，PE-only 阶段无 ELF 声明，显式允许并打印）；
  7. 生成器确定性：重跑到临时文件，`total` 与 `(name,line,module,kind)` 集合一致；
  8. `--neg` 自检：删一条同行声明 / 改 source_sha256 / 塞一条 unparsed ⟹ 必须红。

用法: python scripts/check_state_map.py [--neg]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import copy
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')
MAP = os.path.join(ROOT, 'state_map.json')
MODULES = ['M%d' % i for i in range(10)]
ID = r'[A-Za-z_\u4e00-\u9fff][\w\u4e00-\u9fff]*'


def mask(src):
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        if src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif src.startswith('//', i):
            j = src.find('\n', i + 2)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif src[i] in ('"', "'"):
            q = src[i]
            j = i + 1
            while j < n:
                if src[j] == '\\':
                    j += 2
                    continue
                if src[j] == q:
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


def top_commas(s):
    n, dep = 1, 0
    for ch in s:
        if ch in '([{':
            dep += 1
        elif ch in ')]}':
            dep -= 1
        elif ch == ',' and dep == 0:
            n += 1
    return n


def independent_count(src):
    """独立计数器：行 + 顶层分号/逗号（与生成器的 token/语句扫描不同路径）。"""
    m = mask(src)
    per_line = {}
    protos = 0
    for ln, line in enumerate(m.splitlines(), 1):
        for seg in line.split(';'):
            mm = re.search(r'(?<![\w\u4e00-\u9fff])静(?=\s)', seg)
            if not mm:
                continue
            s = seg[mm.start():]
            if re.match(r'静\s+构\s*(?:' + ID + r'\s*)?\{', s):
                c = 1
            elif '(' in s:
                c = 1
                if '{' not in s:
                    protos += 1
            else:
                c = top_commas(s)
            per_line[ln] = per_line.get(ln, 0) + c
    return sum(per_line.values()), per_line, protos


def verify(d, src):
    bad = []
    items = d.get('items', [])
    total = d.get('total', -1)
    if d.get('unparsed', -1) != 0:
        bad.append('unparsed=%s != 0' % d.get('unparsed'))
    if total != len(items):
        bad.append('total=%d != items=%d' % (total, len(items)))
    if any(it.get('kind') == 'unparsed' or str(it.get('name', '')).startswith('decl@') for it in items):
        bad.append('存在 unparsed/decl@ 条目')
    if not d.get('scope') or not d.get('excluded'):
        bad.append('缺 scope/excluded 口径')
    h = hashlib.sha256(src.encode('utf-8')).hexdigest()
    if d.get('source_sha256') != h:
        bad.append('source_sha256 不匹配（表对应旧源码）')
    if d.get('source_lines') != len(src.splitlines()):
        bad.append('source_lines=%s != %d' % (d.get('source_lines'), len(src.splitlines())))

    n2, per_line, nproto2 = independent_count(src)
    if n2 <= 0:
        bad.append('独立计数=0')
    elif abs(total - n2) * 100 > n2:
        bad.append('total=%d 与独立计数 %d 差 >1%%' % (total, n2))

    # 逐行：独立计数 >1 的行，条目数不得少
    for ln, exp in sorted(per_line.items()):
        if exp <= 1:
            continue
        got = sum(1 for it in items if it.get('line') == ln)
        if got < exp:
            bad.append('L%d 声明 %d 条 < 独立计数 %d' % (ln, got, exp))

    # 多维数组：逐名核（只看**静 声明段**；结构体字段/注释里的 [][] 不算）
    masked_lines = mask(src).splitlines()
    for ln, line in enumerate(masked_lines, 1):
        for seg in line.split(';'):
            mm = re.search(r'(?<![\w\u4e00-\u9fff])静(?=\s)', seg)
            if not mm:
                continue
            s = seg[mm.start():]
            if re.match(r'静\s+构\s*(?:' + ID + r'\s*)?\{', s):
                continue  # 结构体字段不单独计数
            if '][' not in s.replace(' ', ''):
                continue
            names = re.findall(r'(' + ID + r')\s*(?:\[[^\]]*\]\s*){2,}', s)
            have = {it.get('name') for it in items if it.get('line') == ln}
            for nm in names:
                if nm not in have:
                    bad.append('L%d 多维数组 %s 未入 items' % (ln, nm))

    # 前向声明：至少 94 条（审计基线），且 kind 存在
    nproto = sum(1 for it in items if it.get('kind') == 'static-proto')
    if nproto < 94:
        bad.append('static-proto=%d < 94' % nproto)

    # 模块占比 / 齐全
    by = d.get('by_module', {})
    if total > 0:
        for mod, n in by.items():
            if n * 100 > total * 30:
                bad.append('%s 占比 %d/%d > 30%%' % (mod, n, total))
    for mod in MODULES:
        n = by.get(mod, 0)
        if mod == 'M8':
            if n <= 0:
                bad.append('M8 为空（ELF 后端已落地 --target=elf，M8 应有条目）')
        elif n <= 0:
            bad.append('%s 为空' % mod)
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--neg', action='store_true', help='负例自检（删同行声明/改指纹/塞 unparsed）')
    a = ap.parse_args()
    bad = []
    if not os.path.exists(MAP):
        print('[RED] 缺 state_map.json（先跑 gen_state_map.py）')
        return 1
    d = json.load(io.open(MAP, encoding='utf-8'))
    src = io.open(SRC, encoding='utf-8').read()
    fails = verify(d, src)
    print('%s 声明覆盖/口径/模块 | %s' % ('PASS' if not fails else 'FAIL',
                                         '绿' if not fails else '; '.join(fails[:5])))
    if fails:
        bad.append('verify')

    # 生成器确定性
    w = tmp_dir('qcc_statemap')
    out = os.path.join(w, 'state_map.json')
    r = subprocess.run([sys.executable, os.path.join(HERE, 'gen_state_map.py'), '--out', out],
                       cwd=ROOT, capture_output=True)
    if r.returncode != 0:
        bad.append('regen-rc=%d' % r.returncode)
    else:
        d2 = json.load(open(out, encoding='utf-8'))
        same = (d2.get('total') == d.get('total') and
                set((it['name'], it['line'], it['module'], it['kind']) for it in d2.get('items', [])) ==
                set((it['name'], it['line'], it['module'], it['kind']) for it in d.get('items', [])))
        print('%s 生成器确定性' % ('PASS' if same else 'FAIL'))
        if not same:
            bad.append('nondeterministic')
    shutil.rmtree(w, ignore_errors=True)

    if a.neg:
        # 负例 1：删一条同行声明
        d1 = copy.deepcopy(d)
        target = None
        for it in d1['items']:
            if it['line'] == 19:
                target = it
                break
        if target:
            d1['items'].remove(target)
            d1['total'] = len(d1['items'])
            ok1 = bool(verify(d1, src))
            print('%s 负例：删同行声明 -> 判据 %s' % ('PASS' if ok1 else 'FAIL', '判红' if ok1 else '仍绿'))
            if not ok1:
                bad.append('neg-drop')
        else:
            bad.append('neg-drop-setup')
        # 负例 2：旧源码指纹
        d2 = copy.deepcopy(d)
        d2['source_sha256'] = '0' * 64
        ok2 = bool(verify(d2, src))
        print('%s 负例：旧源码指纹 -> 判据 %s' % ('PASS' if ok2 else 'FAIL', '判红' if ok2 else '仍绿'))
        if not ok2:
            bad.append('neg-stale')
        # 负例 3：塞 unparsed
        d3 = copy.deepcopy(d)
        d3['items'].append({'name': 'decl@1', 'line': 1, 'module': 'M1', 'rule': 'x',
                            'kind': 'unparsed', 'indent': 0, 'array': False, 'enclosing': ''})
        d3['total'] = len(d3['items'])
        ok3 = bool(verify(d3, src))
        print('%s 负例：unparsed 条目 -> 判据 %s' % ('PASS' if ok3 else 'FAIL', '判红' if ok3 else '仍绿'))
        if not ok3:
            bad.append('neg-unparsed')

    print('== state_map 判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
