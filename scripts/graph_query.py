# -*- coding: utf-8 -*-
"""qcc -graph 图谱查询工具 (配套 qcc -graph 旁路导出)

用法:
  python scripts/graph_query.py <graph.json> callers <fn>      # 谁调用 fn
  python scripts/graph_query.py <graph.json> callees <fn>      # fn 调用了谁
  python scripts/graph_query.py <graph.json> chain <fn> [N]    # fn 的调用链 (向上 N 层, 默认 3)
  python scripts/graph_query.py <graph.json> risk [high]       # 结构体布局风险 (默认 high)
  python scripts/graph_query.py <graph.json> fanout            # 扇出最大函数 (调用了最多函数)
  python scripts/graph_query.py <graph.json> fanin             # 扇入最大函数 (被最多函数调用)
  python scripts/graph_query.py <graph.json> funcs             # 列出所有用户函数 (非库)
  python scripts/graph_query.py <graph.json> isolated          # 孤立函数 (无调用边)
  python scripts/graph_query.py <graph.json> search <子串>      # 名称/ID 含子串的实体 (函数+结构体)
  python scripts/graph_query.py <graph.json> stats             # 图规模与分布

任何命令后加 `--json` 都输出机器可读 JSON（AI 接口契约；ORD-20261103-99）。
契约 = `-graph` 导出的同一份 JSON schema（entities / relations / layout_sensitive），不新造格式。
"""
import json
import sys

LIB_FNS = (
    'fn:realloc', 'fn:free', 'fn:exit', 'fn:abort', 'fn:fseek', 'fn:ftell', 'fn:rewind',
    'fn:strstr', 'fn:strchr', 'fn:atof', 'fn:strcpy', 'fn:strcat', 'fn:strncmp',
    'fn:isdigit', 'fn:isalpha', 'fn:isalnum', 'fn:isxdigit', 'fn:isspace', 'fn:strrchr',
    'fn:memchr', 'fn:memmove', 'fn:atoi', 'fn:atol', 'fn:strtol', 'fn:strtoul',
    'fn:_strtoui64', 'fn:strcspn', 'fn:strspn', 'fn:strdup', 'fn:strndup', 'fn:qsort',
    'fn:bsearch', 'fn:_scanf_rt', 'fn:malloc', 'fn:memcpy', 'fn:_exit_proc', 'fn:_setpos',
    'fn:_getpos', 'fn:cmp', 'fn:_bump_top', 'fn:_bump_set')


def load(path):
    return json.load(open(path, encoding='utf-8'))


def is_lib(fn):
    return fn.startswith('fn:_') or fn in LIB_FNS


def emit(obj, as_json, text_lines):
    if as_json:
        print(json.dumps(obj, ensure_ascii=False, indent=1))
    else:
        for l in text_lines:
            print(l)


def main():
    as_json = '--json' in sys.argv
    argv = [a for a in sys.argv[1:] if a != '--json']
    if len(argv) < 2:
        print(__doc__)
        return 1
    path, cmd = argv[0], argv[1]
    g = load(path)
    rel = g.get('relations', [])
    ents = g.get('entities', [])
    fns = {e['id'] for e in ents if e.get('kind') == 'function'}

    def arg(i, default=''):
        return argv[i] if len(argv) > i else default

    if cmd == 'callers':
        target = arg(2)
        t = target if target.startswith('fn:') else 'fn:' + target
        c = sorted({r['from'] for r in rel if r['to'] == t})
        emit({'target': t, 'callers': c, 'count': len(c)}, as_json,
             ['callers of %s (%d):' % (t, len(c))] + ['  ' + f for f in c])
    elif cmd == 'callees':
        target = arg(2)
        t = target if target.startswith('fn:') else 'fn:' + target
        c = sorted({r['to'] for r in rel if r['from'] == t})
        emit({'target': t, 'callees': c, 'count': len(c)}, as_json,
             ['callees of %s (%d):' % (t, len(c))] + ['  ' + f for f in c])
    elif cmd == 'chain':
        target = arg(2)
        depth = int(arg(3, '3'))
        t = target if target.startswith('fn:') else 'fn:' + target
        cur, seen, levels = {t}, {t}, []
        for d in range(depth):
            nxt = set()
            for r in rel:
                if r['to'] in cur and r['from'] not in seen:
                    nxt.add(r['from'])
            if not nxt:
                break
            levels.append(sorted(nxt))
            seen |= nxt
            cur = nxt
        lines = ['call chain upward from %s:' % t]
        for i, lv in enumerate(levels):
            lines.append('  level %d: %s' % (i + 1, ', '.join(lv)))
        emit({'target': t, 'levels': levels}, as_json, lines)
    elif cmd == 'risk':
        lvl = arg(2, 'high')
        ss = [s for s in g.get('layout_sensitive', []) if s['risk'] == lvl]
        emit({'risk': lvl, 'structs': ss, 'count': len(ss)}, as_json,
             ['structs risk=%s (%d):' % (lvl, len(ss))] +
             ['  %s fields=%d' % (s['type'], s['fields']) for s in ss])
    elif cmd == 'fanout':
        d = {}
        for r in rel:
            d[r['from']] = d.get(r['from'], 0) + 1
        top = sorted(d.items(), key=lambda x: (-x[1], x[0]))[:10]
        emit({'fanout': [{'fn': f, 'n': n} for f, n in top]}, as_json,
             ['top fanout (函数调用了多少函数):'] + ['  %s: %d' % (f, n) for f, n in top])
    elif cmd == 'fanin':
        d = {}
        for r in rel:
            d[r['to']] = d.get(r['to'], 0) + 1
        top = sorted(d.items(), key=lambda x: (-x[1], x[0]))[:10]
        emit({'fanin': [{'fn': f, 'n': n} for f, n in top]}, as_json,
             ['top fanin (被多少函数调用):'] + ['  %s: %d' % (f, n) for f, n in top])
    elif cmd == 'funcs':
        users = sorted(f for f in fns if not is_lib(f))
        emit({'functions': users, 'count': len(users)}, as_json,
             ['user functions (%d):' % len(users)] + ['  ' + f for f in users])
    elif cmd == 'isolated':
        touched = set()
        for r in rel:
            touched.add(r['from'])
            touched.add(r['to'])
        iso = sorted(f for f in fns if f not in touched)
        emit({'isolated': iso, 'count': len(iso)}, as_json,
             ['isolated functions (%d):' % len(iso)] + ['  ' + f for f in iso])
    elif cmd == 'search':
        sub = arg(2)
        if not sub:
            print('用法: graph_query.py <graph.json> search <子串>'); return 1
        hits = [e for e in ents if sub in e.get('name', '') or sub in e.get('id', '')]
        hit_ids = {e['id'] for e in hits}
        hr = [r for r in rel if r['from'] in hit_ids and r['to'] in hit_ids]
        emit({'query': sub, 'entities': hits, 'relations': hr,
              'count': len(hits)}, as_json,
             ['entities matching %r (%d):' % (sub, len(hits))] +
             ['  %s  (%s)' % (e['id'], e.get('kind', '')) for e in hits])
    elif cmd == 'stats':
        kinds = {}
        for e in ents:
            kinds[e.get('kind', '?')] = kinds.get(e.get('kind', '?'), 0) + 1
        obj = {'entities': len(ents), 'relations': len(rel),
               'layout_sensitive': len(g.get('layout_sensitive', [])), 'kinds': kinds}
        emit(obj, as_json, ['entities=%d relations=%d layout_sensitive=%d'
                            % (obj['entities'], obj['relations'], obj['layout_sensitive'])] +
             ['  %s: %d' % (k, v) for k, v in sorted(kinds.items())])
    else:
        print('未知命令:', cmd)
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
