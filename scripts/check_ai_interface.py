# -*- coding: utf-8 -*-
"""AI 适配接口判据（2026-10-18 · 地基长线）。

驱动 **已发布** 编译器，覆盖五个可独立执行的节（`--only` 分级，2026-10-24 追加）：
  ① help     —— `--help` 列出全部承诺开关（usage 缺项即红）；
  ② cases    —— 每个开关都能实际跑（无「无法打开」）；
  ③ errbytes —— 同一条错误信息 UTF-8 或 GBK 至少一种可读；
  ④ graph    —— `-graph` 实体行号非 0 且与源码一致（用户函数）；
  ⑤ zh       —— 中文开关 `--自检`/`--重复` 与 ASCII `--selfcheck`/`--repeat` 等价（`--重复` 用 `-c` 模式）；
  ⑥ repeat   —— 链接模式 `--repeat 2` 不再 0xC0000005（可重入；残余见台账 D38）。

分级由来：原脚本把六件事并成一个 rc ⟹ 一条链接模式可重入缺陷把 help/graph/中文开关
全部拖成 blocked，台账无法逐行判定。现每节独立可跑，逐行判据各取一节。

用法:
  python scripts/check_ai_interface.py --cc qcc_zh_v2.exe             # 全部（等价旧行为）
  python scripts/check_ai_interface.py --cc qcc_zh_v2.exe --only help # 只验 usage 清单
  python scripts/check_ai_interface.py --cc qcc_zh_v2.exe --only graph,zh
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
    ap.add_argument('--only', default='',
                    help='只跑指定节 (逗号分隔): help,cases,errbytes,graph,zh,repeat; 空 = 全部')
    a = ap.parse_args()
    only = set(x for x in a.only.split(',') if x)

    def on(name):
        return (not only) or (name in only)

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tempfile.mkdtemp(prefix='qcc_ai_iface_')
    src = os.path.join(w, 'p.c')
    bad = os.path.join(w, 'bad.c')
    hdr = os.path.join(w, 'pre.h')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(PROBE)
    io.open(bad, 'w', encoding='utf-8', newline='\n').write(BAD)
    io.open(hdr, 'w', encoding='utf-8', newline='\n').write(HEADER)
    out = os.path.join(w, 'o.exe')
    obj = os.path.join(w, 'o.o')
    fails = []

    expected = ['-S', '-c', '-g', '-bin', '-o', '-I', '-D', '-graph', '--lenient',
                '--dag', '--checkpoint', '--serve', '--interface', '--refs', '--recorder',
                '--annotations', '--diagnostics=json', '--dj', '--budget', '--cache',
                '--cache-verify', '--macro-expand', '--asm-trace', '--selfcheck',
                '--repeat', '--test', '--help']
    help_n = 0

    # ① help 清单
    if on('help'):
        r = run([cc, '--help'], w)
        hb = r.stdout + r.stderr
        try:
            help_txt = hb.decode('utf-8')
        except UnicodeDecodeError:
            help_txt = hb.decode('gbk', 'replace')
        for sw in expected:
            if sw not in help_txt:
                fails.append('help 未列出 %s' % sw)
        help_n = len(expected)

    # ② 每个开关实际可跑
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
        ('--test', [cc, '--test'], 0),
        ('--help', [cc, '--help'], 0),
    ]
    if on('cases'):
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

    # ③ 错误字节
    readable = False
    if on('errbytes'):
        q = run([cc, bad], w)
        eb = q.stdout + q.stderr
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

    # ④ -graph 行号
    graph = {'entities': []}
    zeros = []
    if on('graph'):
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
            if e is None:
                fails.append('-graph 缺实体 %s（实体: %s）' % (eids[0], sorted(ents)[:8]))
                continue
            want = line_of(needle)
            if e.get('line', 0) != want:
                fails.append('%s line=%r 期望 %d' % (e['id'], e.get('line'), want))
        zeros = [e['id'] for e in graph.get('entities', []) if not e.get('line')]
        if zeros:
            fails.append('-graph 仍有 line=0 实体: %s' % zeros[:5])

        # ③b（D19）: 运行库 找不到 时，-graph 行号 仍须 与源码 一致（不许 因 缺运行库 错位）
        g2 = os.path.join(w, 'g2.json')
        env2 = dict(os.environ)
        env2['QCC_RT_PATH'] = os.path.join(w, 'no_such_rt.c')
        try:
            q2 = subprocess.run([cc, '-graph', g2, src, '-o', out], cwd=w,
                                capture_output=True, timeout=180, env=env2)
            g2d = json.load(io.open(g2, encoding='utf-8'))
            ents2 = {e['id']: e for e in g2d.get('entities', [])}
            e2 = next((ents2[x] for x in ('fn:主', 'fn:main') if x in ents2), None)
            want2 = line_of('函数 整 主(')
            if e2 is None:
                fails.append('D19: 缺运行库时 -graph 缺 fn:主')
            elif e2.get('line') != want2:
                fails.append('D19: 缺运行库时 fn:主 line=%r 期望 %d' % (e2.get('line'), want2))
        except Exception as e:
            fails.append('D19: 缺运行库时 -graph 失败/不可解析: %s' % e)

    # ⑤ 中文开关等价（--重复 用 -c 模式：链接模式可重入崩是独立缺陷 D38，不属本节）
    zh1 = zh2 = en1 = en2 = None
    if on('zh'):
        zh1 = run([cc, '--自检', 'deadbeef', src, '-o', out], w)
        en1 = run([cc, '--selfcheck', 'deadbeef', src, '-o', out], w)
        if zh1.returncode != en1.returncode:
            fails.append('--自检 rc=%d != --selfcheck rc=%d' % (zh1.returncode, en1.returncode))
        zh2 = run([cc, '-c', '--重复', '2', src, '-o', obj], w)
        en2 = run([cc, '-c', '--repeat', '2', src, '-o', obj], w)
        if zh2.returncode != 0 or en2.returncode != 0:
            fails.append('-c --重复/--repeat rc=%d/%d (应 0)' % (zh2.returncode, en2.returncode))
        if b'[REPEAT]' not in (zh2.stderr + zh2.stdout) or b'[REPEAT]' not in (en2.stderr + en2.stdout):
            fails.append('-c --重复/--repeat 未输出 [REPEAT] 对账行')

    # ⑥ 链接模式 --repeat（可重入；D38 残余）
    lk_rc = None
    if on('repeat'):
        lk = run([cc, '--repeat', '2', src, '-o', out], w, timeout=240)
        lk_rc = lk.returncode
        if lk_rc != 0:
            fails.append('--repeat(链接模式) rc=%d (应 0; 3221225477/0xC0000005 = 可重入崩)' % lk_rc)

    print('== AI 适配接口判据 ==')
    if on('help'):
        print('help 开关数: %d（断言列出 %d 项）' % (help_n, len(expected)))
    if on('graph'):
        print('graph 实体: %d · line=0: %d' % (len(graph.get('entities', [])), len(zeros)))
    if on('errbytes'):
        print('错误字节: UTF-8/GBK 至少一种可读 = %s' % ('OK' if readable else 'FAIL'))
    if on('zh'):
        print('中文开关等价: --自检=%d/--selfcheck=%d · -c --重复=%d/-c --repeat=%d'
              % (zh1.returncode, en1.returncode, zh2.returncode, en2.returncode))
    if lk_rc is not None:
        print('链接模式 --repeat rc=%d' % lk_rc)
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
