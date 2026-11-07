#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_report_consistency —— 报告一致性闸。

唯一事实源 = `_evidence/20261106_elf_selfhost/FACTS.json`（由 gen_elf_selfhost_facts.py 测量产生）。

检查：
  (a) README.md / FINAL.md 与生成器渲染结果逐字节相同；
  (b) 交接 §十、plan Results 的 `<!-- FACTS-BEGIN -->…<!-- FACTS-END -->` 与生成块逐字节相同；
  (c) 当前态正文（README / FINAL / 交接 §十 / plan Results）不得出现旧 hash，
      除非该行标「历史 / 基线 / 旧 / 演进 / 此前」；
  (d) 任何 8/64 位 hex 必须在白名单（FACTS 里的 hash + git 近期提交号 + `4A0D5E28` 历史基线）内。

用法: python scripts/check_report_consistency.py [--self-test]
退出码: 0 = 全绿；1 = 有红。
"""
import io
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_elf_selfhost_facts as G  # noqa: E402

FACTS = os.path.join(G.EV, 'FACTS.json')
OLD = ['8F443019', '7a153cae', 'B3D3597D', 'bc1e2cc1', '07A9A4D9']
HIST_RE = re.compile(r'历史|基线|旧|演进|此前|当时|上一代')
# 8 位 hex 必须含至少一个字母（排除 20261106 / 18964760 这类纯数字）；
# 64 位 sha256 不受此限（真随机串里纯数字极罕见，但保留完整匹配）。
HEX_RE = re.compile(r'\b(?=[0-9A-Fa-f]*[A-Fa-f])[0-9A-Fa-f]{8}\b|\b[0-9A-Fa-f]{64}\b')


def read_raw(p):
    return io.open(p, 'r', encoding='utf-8', newline='').read()


def whitelist(f):
    s = set()

    def add(v):
        if v:
            s.add(v)
            s.add(v[:8].upper())
            s.add(v[:8].lower())

    add(f['src']['sha256'])
    add(f['windows']['v1'])
    add(f['elf_compiler']['sha256'])
    add(f['tiny']['sha256'])
    add(f['v1']['sha256'])
    for k in ('fixed_point', 'v1', 'v2', 'v3'):
        add(f['elf'].get(k))
    s.add('4A0D5E28')  # 「一处如实交代」里的历史基线
    for h in ('4c388a3b', '0088676a', '6bf931b4'):
        s.add(h)       # 交接 §十 记录的历史演进产物（非当前值）
    for repo in (G.ROOT, G.GROK):
        try:
            out = subprocess.run(['git', 'log', '-20', '--format=%h'], cwd=repo,
                                 capture_output=True, text=True).stdout
            for h in out.split():
                s.add(h)
        except Exception:
            pass
        # 任何存在的 ref（含分支与远端跟踪分支）的 tip 都是本仓真实提交 ⟹ 允许出现在证据里。
        # 由来：plan 里写了发布镜像的 tip `b2b04f19`（在 publish 线上，不在 main 的近 20 笔里），
        # 旧白名单只收 `git log -20`，于是把一句真实记录判成「未登记 hex」。
        try:
            out = subprocess.run(['git', 'for-each-ref', '--format=%(objectname)'],
                                 cwd=repo, capture_output=True, text=True).stdout
            for h in out.split():
                s.add(h[:8])
                s.add(h[:8].upper())
                s.add(h[:8].lower())
        except Exception:
            pass
        # 全 ref 的近期提交（`--all`）也算：证据里引用的常是「某条线的历史提交」，
        # 只收 tip 会把 `316db71d`（origin/publish 的父提交）误判成未登记。
        try:
            out = subprocess.run(['git', 'log', '--all', '-80', '--format=%h'], cwd=repo,
                                 capture_output=True, text=True).stdout
            for h in out.split():
                s.add(h)
        except Exception:
            pass
    return s


def scan_current(text, wl):
    errs = []
    for ln, line in enumerate(text.splitlines(), 1):
        for o in OLD:
            if o in line and not HIST_RE.search(line):
                errs.append('旧 hash `%s` @L%d: %s' % (o, ln, line.strip()[:90]))
        if HIST_RE.search(line):
            continue  # 规则(d) 与 规则(c) 同口径：显式标「历史/基线/旧/演进/此前/当时/上一代」的行不追具体 hash
        for m in HEX_RE.finditer(line):
            tok = m.group(0)
            if tok.upper() not in wl and tok.lower() not in wl and tok not in wl:
                errs.append('未登记 hex `%s` @L%d: %s' % (tok, ln, line.strip()[:90]))
    return errs


def extract_block(text, path):
    if G.BEGIN not in text or G.END not in text:
        return None, '缺 FACTS-BEGIN/END 标记: %s' % path
    a = text.index(G.BEGIN)
    b = text.index(G.END) + len(G.END)
    return text[a:b], None


def section_from(text, marker):
    i = text.find(marker)
    return text[i:] if i >= 0 else ''


def ledger_readme_errors():
    """台账证据 README 的「N = in_progress …」行必须与台账唯一机器源一致。

    机器源 = `grok-build/_GOAL_STATE.json` 的 `counts`（`gen_goal_state.py --write` 产出）。
    skeptic #0b 打回点：本闸原来只校验 ELF 证据目录，台账证据 README 写旧了没人管
    （实测曾落后成 `324 / blocked 8`）。
    """
    import glob
    errs = []
    ledger = os.environ.get('QCC_LEDGER_REPO') or os.path.join(os.path.dirname(G.ROOT), 'grok-build')
    gs = os.path.join(ledger, '_GOAL_STATE.json')
    if not os.path.exists(gs):
        return ['台账证据 README：读不到 %s（判据不完整，不得报绿）' % gs]
    try:
        c = json.load(io.open(gs, encoding='utf-8'))['counts']
    except Exception as e:
        return ['台账证据 README：解析 %s 失败 (%r)' % (gs, e)]
    want = (c['total'], c['in_progress'], c['open'], c['blocked'], c['fixed'], c['settled'])
    pat = re.compile(r'(\d+)\s*=\s*in_progress (\d+) / open (\d+) / blocked (\d+) / fixed (\d+) / settled (\d+)')
    for p in sorted(glob.glob(os.path.join(G.ROOT, '_evidence', '2026*', 'README.md'))):
        txt = io.open(p, 'rb').read().decode('utf-8', 'replace')
        for m in pat.finditer(txt):
            got = tuple(int(x) for x in m.groups())
            if got != want:
                errs.append('台账证据 %s 计数 %r != 台账 %r'
                            % (os.path.relpath(p, G.ROOT), got, want))
    return errs


def _current_fixed_point():
    p = os.path.join(G.ROOT, 'scripts', 'build_zh.ps1')
    if not os.path.exists(p):
        return None
    txt = io.open(p, 'rb').read().decode('utf-8', 'surrogateescape')
    m = re.search(r"^\$expected\s*=\s*'([0-9A-Fa-f]{8})'", txt, re.M)
    return m.group(1).upper() if m else None


def round_evidence_errors():
    """轮次证据（`_evidence/2026*/FACTS.json` + 同名 README）的当前态数字必须与机器源一致。

    skeptic 打回点：本闸过去只校验台账证据 README 的「N = in_progress …」一行，
    同目录 `FACTS.json` 里的 fixed_point / ledger / criteria 写旧了照样报绿
    （实测曾停在 `CB774755` / 324 / 252，而现场是 `1D9588B1` / 325 / 253）。
    """
    import glob
    errs = []
    fp = _current_fixed_point()
    if not fp:
        errs.append('轮次证据：读不到 build_zh.ps1 的 $expected（判据不完整，不得报绿）')

    ledger = os.environ.get('QCC_LEDGER_REPO') or os.path.join(os.path.dirname(G.ROOT), 'grok-build')
    counts = None
    tk = os.path.join(ledger, '_TASKLIST.json')
    if os.path.exists(tk):
        try:
            rows = json.load(io.open(tk, encoding='utf-8'))['rows']
            counts = {
                'total': len(rows),
                'fixed': sum(1 for r in rows if r.get('status') == 'fixed'),
            }
        except Exception as e:
            errs.append('轮次证据：解析 _TASKLIST.json 失败 (%r)' % (e,))

    for p in sorted(glob.glob(os.path.join(G.ROOT, '_evidence', '2026*', 'FACTS.json'))):
        rel = os.path.relpath(p, G.ROOT)
        try:
            f = json.load(io.open(p, encoding='utf-8'))
        except Exception as e:
            errs.append('%s 解析失败 (%r)' % (rel, e))
            continue
        c = f.get('compiler') or {}
        if fp and c.get('fixed_point') and c['fixed_point'].upper() != fp:
            errs.append('%s: 不动点写的是 %s，现场 $expected = %s'
                        % (rel, c['fixed_point'], fp))
        lg = f.get('ledger') or {}
        if counts:
            if 'total' in lg and lg['total'] != counts['total']:
                errs.append('%s: 台账总数 %s != %s' % (rel, lg['total'], counts['total']))
            if 'fixed' in lg and lg['fixed'] != counts['fixed']:
                errs.append('%s: 台账 fixed %s != %s' % (rel, lg['fixed'], counts['fixed']))
        cr = f.get('criteria') or {}
        if counts and 'total' in cr and cr['total'] != counts['fixed']:
            errs.append('%s: 判据总数 %s != fixed 行数 %s（run_criteria 只跑 fixed 行）'
                        % (rel, cr['total'], counts['fixed']))
        if 'total' in cr and 'pass' in cr and cr['total'] != cr['pass'] and cr.get('fail') == 0:
            errs.append('%s: 判据 total=%s 但 pass=%s 且 fail=0（自相矛盾）'
                        % (rel, cr['total'], cr['pass']))

        # 同名 README 的「不动点」行
        rd = os.path.join(os.path.dirname(p), 'README.md')
        if os.path.exists(rd):
            for ln, line in enumerate(io.open(rd, encoding='utf-8', errors='replace'), 1):
                m = re.search(r'\|\s*不动点\s*\|\s*\*\*`?([0-9A-Fa-f]{8})`?\*\*', line)
                if m and fp and m.group(1).upper() != fp:
                    errs.append('%s L%d: 不动点写的是 %s，现场 $expected = %s'
                                % (os.path.relpath(rd, G.ROOT), ln, m.group(1), fp))
    return errs


def _sha256(path):
    import hashlib
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def _scratch_seeds():
    out = []
    for k in ('GOAL_SCRATCH', 'QCC_SCRATCH'):
        v = os.environ.get(k)
        if v and os.path.isdir(v):
            out.append(os.path.abspath(v))
    return out


def _discover_scratch_roots():
    """自动定位本机的 goal scratch 树（`<temp>/grok-goal-*/*/`），**不依赖环境变量**。

    由来（skeptic 打回）：审计侧直接跑 `python scripts/check_report_consistency.py`
    时没有 `GOAL_SCRATCH`，`replica_dirs` 会是 0 ⟹ 副本 sha 校验等于空转。
    这里按目录约定自己找；找不到就明写「本机无 scratch 树」，而不是悄悄报 0。
    """
    import glob
    bases = []
    for k in ('TEMP', 'TMP', 'TMPDIR'):
        v = os.environ.get(k)
        if v and os.path.isdir(v):
            bases.append(v)
    roots = []
    for base in dict.fromkeys(bases):
        for g in glob.glob(os.path.join(base, 'grok-goal-*')):
            if not os.path.isdir(g):
                continue
            try:
                subs = sorted(os.listdir(g))
            except OSError:
                continue
            for s in subs:
                p = os.path.join(g, s)
                if os.path.isdir(p) and os.path.abspath(p) not in roots:
                    roots.append(os.path.abspath(p))
    return roots


def _scratch_dirs():
    """全部可校验的 `{SCRATCH}` 根：环境变量 + 自动发现的 goal scratch 树 + 同级兄弟目录。

    实现侧与审计侧拿到的 `{SCRATCH}` 不是同一个路径（`…/implementer` vs `…/skeptic-0`），
    只认环境变量里的那一个 ⟹ 审计读到的目录永远是空的。
    """
    seeds = list(_scratch_seeds()) + _discover_scratch_roots()
    out = []
    for s in seeds:
        if s not in out:
            out.append(s)
    for s in list(out):
        parent = os.path.dirname(s)
        if not parent or not os.path.isdir(parent):
            continue
        for d in sorted(os.listdir(parent)):
            p = os.path.join(parent, d)
            if os.path.isdir(p) and os.path.abspath(p) not in out:
                out.append(os.path.abspath(p))
    return out


def replica_errors():
    """轮次证据的「唯一写者」契约：MANIFEST ↔ 规范源 ↔ 副本，三者必须逐字节相同。

    skeptic 打回点：同一事实曾有 `_evidence/<round>/x.log` 与 `{SCRATCH}/x.log` 两份，
    各自在不同时刻被手工写成 ⟹ 审计读哪份决定看到新还是旧。现在由
    `scripts/publish_round_evidence.py` 一次写全部位置并落 MANIFEST，本函数逐条验。

    副本目录的取舍：环境变量点名的 `{SCRATCH}` **必须**有副本（它是审计要读的那个）；
    同级兄弟目录只在**已被写者铺过证据**（含 MANIFEST 里任一文件）时才校验 ——
    否则每次新建一个空 scratch 都会把闸打红，那是噪声不是信号。
    """
    import glob
    errs = []
    mans = sorted(glob.glob(os.path.join(G.ROOT, '_evidence', '2026*', 'MANIFEST.sha256')))
    if not mans:
        return ['缺 MANIFEST.sha256：轮次证据没有唯一写者（先跑 scripts/publish_round_evidence.py）']
    fp = _current_fixed_point()
    seeds = _scratch_seeds()
    total = mismatch = 0
    checked_dirs = set()

    for man in mans:
        for line in io.open(man, encoding='utf-8'):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            sha, rel = parts[0], parts[1]
            p = os.path.join(G.ROOT, rel.replace('/', os.sep))
            total += 1
            if not os.path.exists(p):
                errs.append('MANIFEST 指向不存在的日志: %s' % rel)
                mismatch += 1
                continue
            got = _sha256(p)
            if got != sha:
                errs.append('规范源与 MANIFEST 不符（被手改过？）: %s' % rel)
                mismatch += 1
                continue
            first = io.open(p, encoding='utf-8', errors='replace').readline().strip()
            m = re.match(r'# cc=([0-9A-Fa-f]{8})', first)
            if not m:
                errs.append('%s 首行缺 `# cc=<8hex>` 头（不是唯一写者产出）' % rel)
                mismatch += 1
            elif fp and m.group(1).upper() != fp:
                errs.append('%s 首行 cc=%s ≠ 当前 %s（旧日志）' % (rel, m.group(1), fp))
                mismatch += 1

            name = os.path.basename(p)
            dirs = list(seeds)
            for d in _scratch_dirs():
                if d in dirs:
                    continue
                if os.path.exists(os.path.join(d, name)) or any(
                        os.path.exists(os.path.join(d, o)) for o in _manifest_names(mans)):
                    dirs.append(d)
            for d in dirs:
                checked_dirs.add(d)
                q = os.path.join(d, name)
                if not os.path.exists(q):
                    errs.append('副本缺失: %s' % q)
                    mismatch += 1
                elif _sha256(q) != got:
                    errs.append('副本与规范源不一致: %s' % q)
                    mismatch += 1

    # 126③④：failopen 日志不允许用 SKIP 冒充验证
    f = os.path.join(G.ROOT, '_evidence', '20261106_ledger_closure', 'failopen_negative.log')
    if os.path.exists(f):
        for i, ln in enumerate(io.open(f, encoding='utf-8', errors='replace'), 1):
            if ln.startswith('[SKIP]'):
                errs.append('failopen_negative.log L%d 仍含 SKIP（对象缺席 ≠ 已验证）' % i)

    if checked_dirs:
        print('COUNTS files=%d mismatched=%d replica_dirs=%d'
              % (total, mismatch, len(checked_dirs)))
    else:
        print('COUNTS files=%d mismatched=%d replica_dirs=0（本机无 goal scratch 树 —— '
              '副本检查不适用；规范源仍按 MANIFEST 逐份校验）' % (total, mismatch))
    return errs


def _manifest_names(mans):
    """MANIFEST 里出现过的文件名集合（用于判断某个 scratch 目录是否「被铺过证据」）。"""
    names = set()
    for man in mans:
        for line in io.open(man, encoding='utf-8'):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if len(parts) >= 2:
                names.add(os.path.basename(parts[1]))
    return names


def main():
    if not os.path.exists(FACTS):
        print('[FAIL] 缺 FACTS.json：先跑 scripts/gen_elf_selfhost_facts.py')
        return 1
    f = json.load(io.open(FACTS, encoding='utf-8'))
    wl = whitelist(f)
    errs = []

    # (a) whole-file renders
    for name, render in (('README.md', G.render_readme), ('FINAL.md', G.render_final)):
        path = os.path.join(G.EV, name)
        if not os.path.exists(path):
            errs.append('缺文件: %s' % path)
            continue
        want = render(f)
        got = read_raw(path)
        if want != got:
            errs.append('%s 与渲染结果不一致（%d vs %d bytes）' % (name, len(want), len(got)))

    # (b) block documents
    want_block = G.facts_block(f).rstrip('\n')
    hp = os.path.join(G.ROOT, '_交接_ELF自举_20261106.md')
    if os.path.exists(hp):
        got, e = extract_block(read_raw(hp), hp)
        if e:
            errs.append(e)
        elif got.rstrip('\n') != want_block:
            errs.append('交接 FACTS 块与事实源不一致')
    else:
        errs.append('缺交接文档')

    if os.path.exists(G.PLAN):
        got, e = extract_block(read_raw(G.PLAN), G.PLAN)
        if e:
            errs.append(e)
        elif got.rstrip('\n') != want_block:
            errs.append('plan FACTS 块与事实源不一致')
    else:
        errs.append('缺 plan.md')

    # (f) 台账证据 README 的计数一致性（skeptic #0b：本闸原来只盯 ELF 证据，没覆盖它）
    errs += ledger_readme_errors()
    # (g) 轮次证据 FACTS.json / README 的当前态数字（不动点 / 台账 / 判据数）
    errs += round_evidence_errors()
    # (h) 轮次证据的副本契约：MANIFEST ↔ 规范源 ↔ {SCRATCH}，三处逐字节相同
    errs += replica_errors()

    # (c)(d) current-state prose scan
    for name in ('README.md', 'FINAL.md'):
        errs += ['%s: %s' % (name, x) for x in scan_current(read_raw(os.path.join(G.EV, name)), wl)]
    if os.path.exists(hp):
        errs += ['交接§十: %s' % x for x in scan_current(section_from(read_raw(hp), '## 十、'), wl)]
    if os.path.exists(G.PLAN):
        errs += ['plan: %s' % x for x in scan_current(section_from(read_raw(G.PLAN), '## Results'), wl)]

    # (e) SUPERSEDED.md: frozen first-final verbatim + old->new mapping with git proof
    sup = os.path.join(G.EV, 'SUPERSEDED.md')
    froz = os.path.join(G.EV, G.FROZEN)
    if not os.path.exists(sup):
        errs.append('缺 SUPERSEDED.md（冻结首声明与旧→新映射）')
    else:
        st = read_raw(sup)
        fr = io.open(froz, encoding='utf-8').read() if os.path.exists(froz) else ''
        if not fr:
            errs.append('缺 %s（冻结首声明原文）' % G.FROZEN)
        elif fr not in st:
            errs.append('SUPERSEDED.md 未原样收录冻结首声明')
        for old in ('B3D3597D', 'bc1e2cc1', '07A9A4D9', '82f804cb', '54a55181', '49056b5c'):
            if ('`%s' % old) not in st:
                errs.append('SUPERSEDED.md 映射表缺 `%s' % old)
        for line in st.splitlines():
            if line.startswith('| `') and 'cat-file -e' in line and 'rc=0' not in line:
                errs.append('SUPERSEDED.md 映射行 rc!=0: %s' % line[:90])

    # (f) self-test: the scanner must catch an injected stale hash
    if '--self-test' in sys.argv:
        probe = '这句话里藏着 B3D3597D 但没标注'
        if not scan_current(probe, wl):
            errs.append('自检失败：扫描器抓不到注入的旧 hash')
        # 自检 2：轮次证据写了旧不动点必须被抓到（临时造一份，跑完删）
        import shutil
        tmpd = os.path.join(G.ROOT, '_evidence', '2026_selftest_round')
        try:
            os.makedirs(tmpd, exist_ok=True)
            io.open(os.path.join(tmpd, 'FACTS.json'), 'w', encoding='utf-8', newline='\n').write(
                '{"compiler": {"fixed_point": "DEADBEEF"}, "ledger": {"total": 1, "fixed": 1},'
                ' "criteria": {"total": 9, "pass": 8, "fail": 0}}')
            io.open(os.path.join(tmpd, 'README.md'), 'w', encoding='utf-8', newline='\n').write(
                '| 项 | 值 |\n|:--|:--|\n| 不动点 | **`DEADBEEF`**（假的） |\n')
            got = round_evidence_errors()
            if not got:
                errs.append('自检失败：轮次证据写旧值时 round_evidence_errors 没抓到')
            if not any('L3' in g for g in got):
                errs.append('自检失败：README 的不动点行（L3）没被 round_evidence_errors 抓到')
        finally:
            shutil.rmtree(tmpd, ignore_errors=True)

    if errs:
        print('=== 报告一致性闸: FAIL (%d) ===' % len(errs))
        for e in errs[:40]:
            print('  -', e)
        return 1
    print('=== 报告一致性闸: PASS（四份文档与 FACTS.json 逐字节一致；无旧 hash；hex 全部登记）===')
    print('固定点：win=%s elf=%s' % (f['windows']['v1'], f['elf']['fixed_point']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
