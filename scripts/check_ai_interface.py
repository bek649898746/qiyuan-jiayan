# -*- coding: utf-8 -*-
"""AI 适配接口判据（2026-10-18 · 地基长线）。

驱动 **已发布** 编译器，覆盖：
  ① `--help` 列出的每个开关都能实际跑（无「无法打开」）；
  ② 同一条错误信息 UTF-8 或 GBK 至少一种可读；
  ③ `-graph` 实体行号非 0 且与源码一致（用户函数/结构体）；
  ④ 中文开关 `--自检`/`--重复` 与 ASCII `--selfcheck`/`--repeat` 等价；
  ⑤ `--repeat 2` 不再 0xC0000005（可重入堆水位）。
用法: python scripts/check_ai_interface.py --cc qcc_zh_v2.exe
"""
import argparse
import io
import json
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROBE = '''#包含 <stdio.h>
构 学生 { 整 分数; }
整 加一(整 x) { 返 x + 1; }
函数 整 主(空) {
    构 学生 s;
    s.分数 = 加一(6);
    格式化 "AI %d\\n", s.分数;
    返 0;
}
'''

BAD = '''#包含 <stdio.h>
函数 整 主(空) { 未定义名(1); 返 0; }
'''

HEADER = '''static int 预包含 = 7;
'''


def run(args, cwd, timeout=120, stdin=None):
    return subprocess.run(args, cwd=cwd, capture_output=True, timeout=timeout,
                          input=stdin)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tempfile.mkdtemp(prefix='qcc_ai_iface_')
    src = os.path.join(w, 'p.c')
    bad = os.path.join(w, 'bad.c')
    hdr = os.path.join(w, 'pre.h')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(PROBE)
    io.open(bad, 'w', encoding='utf-8', newline='\n').write(BAD)
    io.open(hdr, 'w', encoding='utf-8', newline='\n').write(HEADER)
    fails = []

    # ① help 列出 + 每条可跑（硬编码按 usage 的承诺；usage 缺项也会在这里红）
    r = run([cc, '--help'], w)
    hb = r.stdout + r.stderr
    try:
        help_txt = hb.decode('utf-8')
    except UnicodeDecodeError:
        help_txt = hb.decode('gbk', 'replace')
    expected = ['-S', '-c', '-g', '-bin', '-o', '-I', '-D', '-graph', '--lenient',
                '--dag', '--checkpoint', '--serve', '--interface', '--refs', '--recorder',
                '--annotations', '--diagnostics=json', '--dj', '--budget', '--cache',
                '--cache-verify', '--macro-expand', '--asm-trace', '--selfcheck',
                '--repeat', '--test', '--help']
    for s in expected:
        if s not in help_txt:
            fails.append('help 未列出 %s' % s)

    out = os.path.join(w, 'o.exe')
    obj = os.path.join(w, 'o.o')
    cases = [
        ('-S', [cc, '-S', src, '-o', os.path.join(w, 'o.asm')], 0),
        ('-c', [cc, '-c', src, '-o', obj], 0),
        ('-g', [cc, '-g', src, '-o', out], 0),
        ('-bin', [cc, '-bin', src, '-o', os.path.join(w, 'o.bin')], 0),
        ('-o', [cc, src, '-o', out], 0),
        ('-I', [cc, '-I', hdr, src, '-o', out], 0),
        ('-D', [cc, '-D', 'X=1', src, '-o', out], 0),
        ('-graph', [cc, '-graph', os.path.join(w, 'g.json'), src, '-o', out], 0),
        ('--lenient', [cc, '--lenient', src, '-o', out], 0),
        ('--dag', [cc, '--dag', src, '-o', out], 0),
        ('--checkpoint', [cc, '--checkpoint', os.path.join(w, 'cp.json'), src, '-o', out], 0),
        ('--serve', [cc, '--serve'], 0),
        ('--interface', [cc, '--interface', src], 0),
        ('--refs', [cc, '--refs', src], 0),
        ('--recorder', [cc, '--recorder', os.path.join(w, 'rec.json'), src, '-o', out], 0),
        ('--annotations', [cc, '--annotations', src], 0),
        ('--diagnostics=json', [cc, '--diagnostics=json', bad], 1),
        ('--dj', [cc, '--dj', bad], 1),
        ('--budget', [cc, '--budget', src, '-o', out], 0),
        ('--cache', [cc, '--cache', src, '-o', out], 0),
        ('--cache-verify', [cc, '--cache-verify', src, '-o', out], 0),
        ('--macro-expand', [cc, '--macro-expand', src, '-o', out], 0),
        ('--asm-trace', [cc, '--asm-trace', src, '-o', out], 0),
        ('--selfcheck', [cc, '--selfcheck', 'deadbeef', src, '-o', out], 1),
        ('--repeat', [cc, '--repeat', '2', src, '-o', out], 0),
        ('--test', [cc, '--test'], 0),
        ('--help', [cc, '--help'], 0),
    ]
    for name, args, want in cases:
        try:
            q = run(args, w, timeout=180, stdin=b'')
            blob = (q.stdout + q.stderr).decode('utf-8', 'replace')
            if want is not None and q.returncode != want:
                fails.append('%s rc=%d 期望 %d: %s' % (name, q.returncode, want, blob[:160]))
            if '无法打开' in blob and name not in ('--selfcheck',):
                fails.append('%s 报「无法打开」: %s' % (name, blob[:160]))
        except subprocess.TimeoutExpired:
            fails.append('%s 超时' % name)

    # ② 错误字节：UTF-8 或 GBK 之一可读
    q = run([cc, bad], w)
    eb = q.stdout + q.stderr
    readable = False
    for enc in ('utf-8', 'gbk'):
        try:
            t = eb.decode(enc)
            if 'A1001' in t and '未声明函数' in t:
                readable = True
                break
        except UnicodeDecodeError:
            pass
    if not readable:
        fails.append('错误信息 UTF-8/GBK 均不可读: %r' % eb[:120])

    # ③ -graph 行号
    g = os.path.join(w, 'g.json')
    run([cc, '-graph', g, src, '-o', out], w)
    try:
        graph = json.load(io.open(g, encoding='utf-8'))
    except Exception as e:
        graph = {'entities': []}
        fails.append('-graph 产物不可解析: %s' % e)
    lines = io.open(src, encoding='utf-8').read().split('\n')

    def line_of(needle):
        for i, l in enumerate(lines, 1):
            if needle in l:
                return i
        return -1
    ents = {e['id']: e for e in graph.get('entities', [])}
    for eids, needle in [(['fn:主', 'fn:main'], '函数 整 主('),
                         (['fn:加一'], '整 加一(')]:
        e = next((ents[x] for x in eids if x in ents), None)
        eid = e['id'] if e else eids[0]
        if e is None:
            fails.append('-graph 缺实体 %s（实体: %s）' % (eids[0], sorted(ents)[:8]))
            continue
        want = line_of(needle)
        if e.get('line', 0) != want:
            fails.append('%s line=%r 期望 %d' % (eid, e.get('line'), want))
    zeros = [e['id'] for e in graph.get('entities', []) if not e.get('line')]
    if zeros:
        fails.append('-graph 仍有 line=0 实体: %s' % zeros[:5])

    # ④ 中文开关等价
    zh1 = run([cc, '--自检', 'deadbeef', src, '-o', out], w)
    en1 = run([cc, '--selfcheck', 'deadbeef', src, '-o', out], w)
    if zh1.returncode != en1.returncode:
        fails.append('--自检 rc=%d != --selfcheck rc=%d' % (zh1.returncode, en1.returncode))
    zh2 = run([cc, '--重复', '2', src, '-o', out], w)
    en2 = run([cc, '--repeat', '2', src, '-o', out], w)
    if zh2.returncode != 0 or en2.returncode != 0:
        fails.append('--重复/--repeat rc=%d/%d (应 0; 0xC0000005 = 可重入崩)'
                     % (zh2.returncode, en2.returncode))
    if b'[REPEAT]' not in (zh2.stderr + zh2.stdout) or b'[REPEAT]' not in (en2.stderr + en2.stdout):
        fails.append('--重复/--repeat 未输出 [REPEAT] 对账行')

    # ⑤ --repeat 可重入梯度（2026-10-20 压测补：原判据只测 N=2，低于实际崩溃点）
    #    实测：N=2/5 通过，N>=10 稳定 0xC0000005（千问审计发现，启元复验一致）。
    #    N=5 设为硬判据；N=10 记为 KNOWN-GAP（修好前不阻塞，但每次打印实际结果）。
    rep5 = run([cc, '--repeat', '5', src, '-o', out], w, timeout=300)
    if rep5.returncode != 0:
        fails.append('--repeat 5 rc=%d（N=5 为硬判据；0xC0000005 = 可重入崩）' % rep5.returncode)
    rep10 = run([cc, '--repeat', '10', src, '-o', out], w, timeout=300)
    repeat_gap = (rep10.returncode != 0)

    print('== AI 适配接口判据 ==')
    print('help 开关数: %d（断言列出 %d 项）' % (len(expected), len(expected)))
    print('graph 实体: %d · line=0: %d' % (len(graph.get('entities', [])), len(zeros)))
    print('错误字节: UTF-8/GBK 至少一种可读 = %s' % ('OK' if readable else 'FAIL'))
    print('中文开关等价: --自检=%d/--selfcheck=%d · --重复=%d/--repeat=%d'
          % (zh1.returncode, en1.returncode, zh2.returncode, en2.returncode))
    print('--repeat 梯度: N=2=%d · N=5=%d · N=10=%d%s'
          % (en2.returncode, rep5.returncode, rep10.returncode,
             '  [KNOWN-GAP: N>=10 段错误，待修]' if repeat_gap else ''))
    if fails:
        print('FAIL=%d' % len(fails))
        for f in fails:
            print('  - ' + f)
        print('work=' + w)
        return 1
    print('=== AI interface: PASS ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
