#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_graph_query —— ORD-20261103-99 判据：知识图谱查询接口（契约 = `-graph` 的 JSON）。

驱动**真实链路**：用当前编译器 `-graph` 导出一份图，再逐条跑 `scripts/graph_query.py` 的命令，
断言结果对得上探针源码；`--json` 必须能被 `json.loads`；未知命令必须 exit≠0（负例）。

探针（`整 加数` / `整 主` 调用它）断言：
  · `funcs` 含 `fn:主` 与 `fn:加数`
  · `callees 主` 含 `fn:加数`；`callers 加数` 含 `fn:主`
  · `search 加数` 命中 1 个实体（中文子串；工具在 Python 层做 UTF-8，不经过 argv 代码页）
  · `stats` 的 entities/relations 与 JSON 实际长度一致

用法: python scripts/check_graph_query.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全过；1 = 任一不符。
"""
import argparse
import io
import json
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, 'scripts', 'graph_query.py')

SRC = '整 加数(整 甲, 整 乙) { 返 甲 + 乙; }\n整 主(空) { 返 加数(1, 2); }\n'


def run_tool(graph, args):
    r = subprocess.run([sys.executable, TOOL, graph] + args, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    return r.returncode, r.stdout, r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    bad = []
    work = tmp_dir('gq')
    try:
        src = os.path.join(work, 'p.c')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
        graph = os.path.join(work, 'g.json')
        env = dict(os.environ)
        env['QCC_RT_PATH'] = os.path.join(ROOT, 'srclib', 'qcc_rt.c')
        r = subprocess.run([cc, src, '-o', os.path.join(work, 'p.exe'), '-graph', graph],
                           cwd=ROOT, capture_output=True, text=True, encoding='utf-8',
                           errors='replace', env=env)
        if r.returncode != 0 or not os.path.exists(graph):
            print('[FAIL] -graph 导出失败 rc=%d %s' % (r.returncode, (r.stderr or '')[:160]))
            return 1
        g = json.load(io.open(graph, encoding='utf-8'))
        names = {e['id'] for e in g.get('entities', [])}
        print('[图] entities=%d relations=%d layout=%d'
              % (len(g.get('entities', [])), len(g.get('relations', [])),
                 len(g.get('layout_sensitive', []))))

        def need(cond, label):
            print('[%s] %s' % ('PASS' if cond else 'FAIL', label))
            if not cond:
                bad.append(label)

        rc, out, _ = run_tool(graph, ['funcs', '--json'])
        fs = json.loads(out) if rc == 0 else {}
        need(rc == 0 and 'fn:主' in fs.get('functions', []) and 'fn:加数' in fs.get('functions', []),
             'funcs 含 fn:主 / fn:加数')

        rc, out, _ = run_tool(graph, ['callees', '主', '--json'])
        cs = json.loads(out) if rc == 0 else {}
        need(rc == 0 and 'fn:加数' in cs.get('callees', []), 'callees 主 → fn:加数')

        rc, out, _ = run_tool(graph, ['callers', '加数', '--json'])
        ck = json.loads(out) if rc == 0 else {}
        need(rc == 0 and 'fn:主' in ck.get('callers', []), 'callers 加数 → fn:主')

        rc, out, _ = run_tool(graph, ['chain', '加数', '2', '--json'])
        ch = json.loads(out) if rc == 0 else {}
        need(rc == 0 and ch.get('levels') and 'fn:主' in ch['levels'][0], 'chain 加数 含 fn:主')

        rc, out, _ = run_tool(graph, ['search', '加数', '--json'])
        se = json.loads(out) if rc == 0 else {}
        need(rc == 0 and any(e['id'] == 'fn:加数' for e in se.get('entities', [])),
             'search 加数（中文子串）命中')

        rc, out, _ = run_tool(graph, ['stats', '--json'])
        st = json.loads(out) if rc == 0 else {}
        need(rc == 0 and st.get('entities') == len(g['entities'])
             and st.get('relations') == len(g['relations']), 'stats 与 JSON 长度一致')

        for c in ('risk', 'fanout', 'fanin', 'isolated'):
            rc, out, _ = run_tool(graph, [c, '--json'])
            ok = rc == 0
            if ok:
                try:
                    json.loads(out)
                except Exception:
                    ok = False
            need(ok, '%s --json 可解析' % c)

        rc, _, _ = run_tool(graph, ['no-such-command'])
        need(rc != 0, '负例：未知命令 exit≠0')
        rc, _, _ = run_tool(graph, ['search'])
        need(rc != 0, '负例：search 缺参数 exit≠0')
    finally:
        shutil.rmtree(work, ignore_errors=True)

    if bad:
        print('=== ORD-20261103-99 判据: FAIL -> %s ===' % bad)
        return 1
    print('=== ORD-20261103-99 判据: PASS（10 项查询 + 2 负例；契约 = -graph JSON）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
