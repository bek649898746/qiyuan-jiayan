# -*- coding: utf-8 -*-
"""中文汇编 基线 / 差异 报告器（R599c-T10b · 优化器的「放大镜」）

优化器一生效，「逐字节不变」的护身符就失效；本工具提供**文本层**的对照物：
按**函数段**与**助记符**分类统计两份 `.asm` 的差异，让「优化真生效 / 只动了预期处」可复核。

用法:
  python scripts/check_asm_baseline.py --record <file.asm> [--tag 名字]
      落 hash + 行数 + 助记符计数基线到 scripts/asm_baseline.json（可复算）
  python scripts/check_asm_baseline.py --diff <base.asm> <new.asm> [--top N]
      输出：总行/字节变化、按助记符计数差、按函数段的指令数差、首个差异行号

退出码: 0 = 报告生成成功（差异本身不是错误）；1 = 用法/文件错误。
"""
import io, os, re, sys, json, hashlib
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
BASE_JSON = os.path.join('scripts', 'asm_baseline.json')

SEC_RE = re.compile(r'^\s*;\s*===\s*(.*?)\s*===')
MN_RE = re.compile(r'^\s+(\S+)')


def scan(path):
    """流式扫一份 .asm：返回 行数/字节数/hash/助记符计数/函数段计数。"""
    h = hashlib.sha256()
    nbytes = nlines = 0
    mn = {}
    funcs = {}
    cur = '<head>'
    fl = {}
    with io.open(path, 'rb') as f:
        for raw in f:
            h.update(raw)
            nbytes += len(raw)
            nlines += 1
            s = raw.decode('utf-8', 'replace').rstrip('\r\n')
            m = SEC_RE.match(s)
            if m:
                cur = m.group(1)
                funcs[cur] = funcs.get(cur, 0) + 1
                continue
            m = MN_RE.match(s)
            if m:
                t = m.group(1)
                if t.startswith('.') or t.startswith(';'):
                    continue
                mn[t] = mn.get(t, 0) + 1
                fl[cur] = fl.get(cur, 0) + 1
    return {'bytes': nbytes, 'lines': nlines, 'sha256_16': h.hexdigest()[:16],
            'mnemonics': mn, 'sections': len(funcs), 'func_lines': fl}


def cmd_record(argv):
    path = argv[0]
    tag = None
    if '--tag' in argv:
        tag = argv[argv.index('--tag') + 1]
    tag = tag or os.path.basename(path)
    r = scan(path)
    data = {}
    if os.path.exists(BASE_JSON):
        data = json.load(io.open(BASE_JSON, encoding='utf-8'))
    data[tag] = {'file': path.replace('\\', '/'), 'bytes': r['bytes'], 'lines': r['lines'],
                 'sha256_16': r['sha256_16'], 'sections': r['sections'],
                 'mnemonics': dict(sorted(r['mnemonics'].items()))}
    with io.open(BASE_JSON, 'w', encoding='utf-8', newline='\n') as f:
        f.write(json.dumps(data, ensure_ascii=False, indent=1, sort_keys=True))
    print('[record] %s -> %s' % (tag, BASE_JSON))
    print('  bytes=%d lines=%d sha256_16=%s sections=%d mn_kinds=%d mn_total=%d' % (
        r['bytes'], r['lines'], r['sha256_16'], r['sections'], len(r['mnemonics']),
        sum(r['mnemonics'].values())))
    return 0


def cmd_diff(argv):
    top = 20
    if '--top' in argv:
        top = int(argv[argv.index('--top') + 1])
    a, b = argv[0], argv[1]
    ra, rb = scan(a), scan(b)
    print('== 中文汇编 diff ==')
    print('  base: %s' % a)
    print('        bytes=%d lines=%d sha256_16=%s sections=%d mn_total=%d' % (
        ra['bytes'], ra['lines'], ra['sha256_16'], ra['sections'], sum(ra['mnemonics'].values())))
    print('  new : %s' % b)
    print('        bytes=%d lines=%d sha256_16=%s sections=%d mn_total=%d' % (
        rb['bytes'], rb['lines'], rb['sha256_16'], rb['sections'], sum(rb['mnemonics'].values())))
    db = rb['bytes'] - ra['bytes']
    dl = rb['lines'] - ra['lines']
    dm = sum(rb['mnemonics'].values()) - sum(ra['mnemonics'].values())
    print('  Δ     bytes=%+d (%+.2f%%)  lines=%+d  mnemonics=%+d' % (
        db, (100.0 * db / ra['bytes']) if ra['bytes'] else 0.0, dl, dm))
    print('  identical: %s' % (ra['sha256_16'] == rb['sha256_16']))
    print('--- 按助记符（|Δ| 最大的 %d 项） ---' % top)
    keys = set(ra['mnemonics']) | set(rb['mnemonics'])
    rows = sorted(((rb['mnemonics'].get(k, 0) - ra['mnemonics'].get(k, 0), k) for k in keys),
                  key=lambda x: -abs(x[0]))
    for d, k in rows[:top]:
        if d == 0:
            continue
        print('  %-16s base=%-8d new=%-8d Δ=%+d' % (k, ra['mnemonics'].get(k, 0),
                                                   rb['mnemonics'].get(k, 0), d))
    print('--- 按函数段（|Δ| 最大的 %d 项） ---' % top)
    keys = set(ra['func_lines']) | set(rb['func_lines'])
    rows = sorted(((rb['func_lines'].get(k, 0) - ra['func_lines'].get(k, 0), k) for k in keys),
                  key=lambda x: -abs(x[0]))
    for d, k in rows[:top]:
        if d == 0:
            continue
        print('  %-40s base=%-8d new=%-8d Δ=%+d' % (k[:40], ra['func_lines'].get(k, 0),
                                                    rb['func_lines'].get(k, 0), d))
    # 首个差异行
    fa = io.open(a, 'r', encoding='utf-8', errors='replace')
    fb = io.open(b, 'r', encoding='utf-8', errors='replace')
    ln = 0
    while True:
        la, lb = fa.readline(), fb.readline()
        if not la and not lb:
            print('--- 首个差异行: 无（逐行相同） ---')
            break
        ln += 1
        if la != lb:
            print('--- 首个差异行: %d ---' % ln)
            print('  base: %r' % la.rstrip('\n'))
            print('  new : %r' % lb.rstrip('\n'))
            break
    fa.close()
    fb.close()
    return 0


if __name__ == '__main__':
    argv = sys.argv[1:]
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__)
        sys.exit(0)
    if argv[0] == '--record':
        sys.exit(cmd_record(argv[1:]))
    if argv[0] == '--diff':
        sys.exit(cmd_diff(argv[1:]))
    print('unknown mode: %s' % argv[0])
    sys.exit(1)
