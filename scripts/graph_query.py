# -*- coding: utf-8 -*-
"""qcc -graph 图谱查询工具 (配套 qcc_x86.c -graph 旁路导出)

用法:
  python scripts/graph_query.py <graph.json> callers <fn>      # 谁调用 fn
  python scripts/graph_query.py <graph.json> callees <fn>      # fn 调用了谁
  python scripts/graph_query.py <graph.json> chain <fn> [N]    # fn 的调用链 (向上 N 层, 默认 3)
  python scripts/graph_query.py <graph.json> risk [high]       # 结构体布局风险 (默认 high)
  python scripts/graph_query.py <graph.json> fanout            # 扇出最大函数 (调用了最多函数)
  python scripts/graph_query.py <graph.json> fanin             # 扇入最大函数 (被最多函数调用)
  python scripts/graph_query.py <graph.json> funcs             # 列出所有用户函数 (非库)
  python scripts/graph_query.py <graph.json> isolated          # 孤立函数 (无调用边)
"""
import json, sys, os

def load(path):
    return json.load(open(path, encoding='utf-8'))

def is_lib(fn):
    return fn.startswith('fn:_') or fn in (
        'fn:realloc','fn:free','fn:exit','fn:abort','fn:fseek','fn:ftell','fn:rewind',
        'fn:strstr','fn:strchr','fn:atof','fn:strcpy','fn:strcat','fn:strncmp',
        'fn:isdigit','fn:isalpha','fn:isalnum','fn:isxdigit','fn:isspace','fn:strrchr',
        'fn:memchr','fn:memmove','fn:atoi','fn:atol','fn:strtol','fn:strtoul',
        'fn:_strtoui64','fn:strcspn','fn:strspn','fn:strdup','fn:strndup','fn:qsort',
        'fn:bsearch','fn:_scanf_rt','fn:malloc','fn:memcpy','fn:_exit_proc','fn:_setpos',
        'fn:_getpos','fn:cmp','fn:_bump_top','fn:_bump_set')

def main():
    if len(sys.argv) < 3:
        print(__doc__); return 1
    path, cmd = sys.argv[1], sys.argv[2]
    g = load(path)
    rel = g.get('relations', [])
    ents = g.get('entities', [])
    fns = {e['id'] for e in ents if e.get('kind') == 'function'}

    if cmd == 'callers':
        target = sys.argv[3] if len(sys.argv) > 3 else ''
        t = target if target.startswith('fn:') else 'fn:' + target
        c = [r['from'] for r in rel if r['to'] == t]
        print('callers of %s (%d):' % (t, len(c)))
        for f in sorted(c): print(' ', f)
    elif cmd == 'callees':
        target = sys.argv[3] if len(sys.argv) > 3 else ''
        t = target if target.startswith('fn:') else 'fn:' + target
        c = [r['to'] for r in rel if r['from'] == t]
        print('callees of %s (%d):' % (t, len(c)))
        for f in sorted(c): print(' ', f)
    elif cmd == 'chain':
        target = sys.argv[3] if len(sys.argv) > 3 else ''
        depth = int(sys.argv[4]) if len(sys.argv) > 4 else 3
        t = target if target.startswith('fn:') else 'fn:' + target
        # 向上回溯调用者
        cur = {t}
        seen = {t}
        print('call chain upward from %s:' % t)
        for d in range(depth):
            nxt = set()
            for r in rel:
                if r['to'] in cur and r['from'] not in seen:
                    nxt.add(r['from'])
            if not nxt: break
            print('  level %d: %s' % (d + 1, ', '.join(sorted(nxt))))
            seen |= nxt
            cur = nxt
    elif cmd == 'risk':
        lvl = sys.argv[3] if len(sys.argv) > 3 else 'high'
        ss = [s for s in g.get('layout_sensitive', []) if s['risk'] == lvl]
        print('structs risk=%s (%d):' % (lvl, len(ss)))
        for s in ss:
            print('  %s fields=%d' % (s['type'], s['fields']))
    elif cmd == 'fanout':
        d = {}
        for r in rel:
            d[r['from']] = d.get(r['from'], 0) + 1
        top = sorted(d.items(), key=lambda x: -x[1])[:10]
        print('top fanout (函数调用了多少函数):')
        for f, n in top:
            print('  %s: %d' % (f, n))
    elif cmd == 'fanin':
        d = {}
        for r in rel:
            d[r['to']] = d.get(r['to'], 0) + 1
        top = sorted(d.items(), key=lambda x: -x[1])[:10]
        print('top fanin (被多少函数调用):')
        for f, n in top:
            print('  %s: %d' % (f, n))
    elif cmd == 'funcs':
        users = [f for f in fns if not is_lib(f)]
        print('user functions (%d):' % len(users))
        for f in sorted(users): print(' ', f)
    elif cmd == 'isolated':
        touched = set()
        for r in rel:
            touched.add(r['from']); touched.add(r['to'])
        iso = [f for f in fns if f not in touched]
        print('isolated functions (%d):' % len(iso))
        for f in sorted(iso): print(' ', f)
    else:
        print('未知命令:', cmd); print(__doc__); return 1
    return 0

if __name__ == '__main__':
    sys.exit(main())
