# -*- coding: utf-8 -*-
"""anchor_sensitivity —— **CORE_FEAT 每个锚「能红」的逐条实跑证据**（台账/plan 判据 2）。

2026-10-24 改版（判据方打回「unproven=0 是定义出来的」）：
  · 旧版把 `content`/`pair` 当证据 —— 那只是 `@EXPECTED` 文本的**静态分类**，不是「能红」的证明。
  · 新版 `--audit`（默认）**逐锚真变异 + 真跑 runner**：把该锚的期望值改成哨兵，
    `run_tests_zh.py --only <锚> --parallel N` 必须 FAIL，随后**字节级还原**并断言还原成功。
  · `unproven` = 没有「基线绿 + 变异红」实跑记录的锚（`@KNOWN-GAP` 单列，因为它本来就必须红）。
    不是文本分类算出来的 —— 是实跑结果算出来的。

变异形式（按优先级取第一条可用的）：
  ① `@EXPECTED out|err|warn:<值>` → 值换成 `__SENS_MUT__`；
  ② `@EXPECTED silent:N` → `N+1`；
  ③ `@EXPECTED exit:N` → `N+1`；
  ④ 只在 `in:` 或裸 `compile_fail` / 无指令时 → 追加 `// @EXPECTED out:__SENS_MUT__`
     （runner 对 `@EXPECTED` 逐条校验；负控锚追加 out 断言后，编译失败仍会因 stderr 不含哨兵而 FAIL）。

批处理 + 二分归属：基线 240 锚/批、变异 120 锚/批；批计数不符（有 SKIP / 个别 STILL-GREEN）
⟹ **二分**递归到单锚，保证每条证据都能归到具体锚，且不必为每个锚付一次进程启动。
编译崩溃（0xC0000005）当场中止，并先还原全部被改字节。

用法:
  python scripts/anchor_sensitivity.py                       # 全量审计（默认）
  python scripts/anchor_sensitivity.py --batch 240 --mbatch 120 --workers 4 --json out.json --log out.log
  python scripts/anchor_sensitivity.py --mutate 10           # 兼容旧用法：只抽样 10 个
  python scripts/anchor_sensitivity.py --focus-file focus.txt # 额外断言这些锚必须 RED 且列出
退出码: 0 = unproven==0（且 focus 全 RED）；1 = 有未证锚/基线不绿/变异未红；2 = 环境未就绪
"""
import argparse
import io
import json
import os
import random
import re
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNNER = os.path.join(ROOT, 'scripts', 'run_tests_zh.py')
SENTINEL = '__SENS_MUT__'
EXP_RE = re.compile(r'//\s*@EXPECTED\s+(.+)')
ASSERT_KINDS = ('out:', 'err:', 'in:', 'warn:', 'silent:')


class Crash(Exception):
    pass


def core_feat():
    out, depth = [], 0
    for line in io.open(RUNNER, encoding='utf-8', errors='replace').read().split('\n'):
        t = line.strip()
        if 'CORE_FEAT' in t and '[' in t:
            depth += t.count('[') - t.count(']')
            continue
        if depth > 0:
            if t.startswith('#') or t.startswith('//'):
                continue
            out += re.findall(r"'(scripts/[^']+\.c|tests/[^']+\.c|behavior/[^']+\.c|qcc/[^']+\.c)'", line)
            depth += t.count('[') - t.count(']')
    return list(dict.fromkeys(out))


def abspath(rel):
    for c in (rel, 'tests/' + rel):
        p = os.path.join(ROOT, c.replace('/', os.sep))
        if os.path.exists(p):
            return p
    return os.path.join(ROOT, rel.replace('/', os.sep))


def stem(rel):
    b = os.path.basename(rel)[:-2]
    for suf in ('_ok', '_neg2', '_neg3', '_neg', '_ok2'):
        if b.endswith(suf):
            return b[:-len(suf)]
    return b


def expectation(path):
    txt = io.open(path, encoding='utf-8', errors='replace').read()
    lines = [m.group(1).strip() for m in EXP_RE.finditer(txt)]
    joined = ' '.join(lines)
    content = any(k in joined and joined.split(k, 1)[1].strip() for k in ASSERT_KINDS)
    runtime_only = bool(re.search(r'@EXPECTED\s+exit:\s*\S+', joined)) or (
        'out:' in joined and joined.split('out:', 1)[1].strip() != '')
    neg = ('compile_fail' in joined) or bool(re.search(r'exit:\s*(?!0\b)\S+', joined)) \
        or ('_neg' in os.path.basename(path) and not runtime_only)
    return {'lines': lines, 'content': content, 'negative': neg, 'none': not lines, 'raw': joined}


def classify(rel, all_anchors, exp):
    e = exp[rel]
    if e['content']:
        return 'content'
    if e['negative']:
        return 'negative'
    sib = [q for q in all_anchors if q != rel and stem(q) == stem(rel) and '_neg' in os.path.basename(q)
           and (exp[q]['content'] or exp[q]['negative'])]
    return 'pair' if sib else 'static-only'


def plan_mutation(text):
    lines = text.split('\n')
    for i, line in enumerate(lines):
        m = re.search(r'(//\s*@EXPECTED\s+)(out|err|warn):(.*)$', line)
        if m and m.group(3).strip():
            lines[i] = line[:m.start(3)] + SENTINEL
            return '\n'.join(lines), m.group(2) + ':sentinel'
    for i, line in enumerate(lines):
        m = re.search(r'(//\s*@EXPECTED\s+silent:\s*)(\d+)', line)
        if m:
            lines[i] = line[:m.start(2)] + str(int(m.group(2)) + 1)
            return '\n'.join(lines), 'silent:+1'
    for i, line in enumerate(lines):
        m = re.search(r'(//\s*@EXPECTED\s+exit:\s*)(\d+)', line)
        if m:
            v = int(m.group(2))
            lines[i] = line[:m.start(2)] + str(0 if v == 65535 else v + 1)
            return '\n'.join(lines), 'exit:+1'
    ins = '// @EXPECTED out:' + SENTINEL
    if lines and lines[0].startswith('\ufeff'):
        lines = [lines[0], ins] + lines[1:]
    else:
        lines = [ins] + lines
    return '\n'.join(lines), 'append out:sentinel'


def run_only(batch, cc, workers, timeout=2400, mutating=False):
    env = dict(os.environ, QCC_TEST_BIN=os.path.abspath(cc))
    if mutating:
        env['QCC_NO_RETRY'] = '1'   # 变异跑失败是预期结果，重试只会翻倍审计时间
    r = subprocess.run([sys.executable, RUNNER, '--only', ','.join(batch), '--parallel', str(workers)],
                       capture_output=True, text=True, encoding='utf-8', errors='replace',
                       timeout=timeout, cwd=ROOT, env=env)
    out = (r.stdout or '') + (r.stderr or '')
    m = re.search(r'PASS=(\d+)\s+SKIP=(\d+)\s+FAIL=(\d+)', out)
    n = {'pass': int(m.group(1)), 'skip': int(m.group(2)), 'fail': int(m.group(3))} if m else None
    return r.returncode, n, out


def crash_evidence(out):
    return ('0xC0000005' in out) or ('3221225477' in out) or ('-1073741819' in out)


def apply_batch(batch):
    saved = []
    for rel in batch:
        p = abspath(rel)
        orig = io.open(p, 'rb').read()
        new, form = plan_mutation(orig.decode('utf-8', errors='replace'))
        io.open(p, 'w', encoding='utf-8', newline='').write(new)
        saved.append((rel, p, orig, form))
    return saved


def restore_batch(saved):
    bad = []
    for rel, p, orig, _ in saved:
        io.open(p, 'wb').write(orig)
        if io.open(p, 'rb').read() != orig:
            bad.append(rel)
    return bad


def parse_status(out, batch):
    """从 runner 输出里读「哪些锚失败 / 跳过」。

    runner 形式（实测）：
      --- N 个失败, 自动重试一次: a b c ---
      --- 显式 SKIP ... ---
        <name>  SKIP: ...
      --- 失败 ---
        <name> (期望exit=...)
    失败段最多列 30 条 ⇹ 超过 30 条时返回 None（交给 二分）。
    """
    m = re.search(r'PASS=(\d+)\s+SKIP=(\d+)\s+FAIL=(\d+)', out)
    if not m:
        return None
    n = {'pass': int(m.group(1)), 'skip': int(m.group(2)), 'fail': int(m.group(3))}
    fails = set(re.findall(r'^ {2}(\S+) \(', out, re.M))
    skips = set(re.findall(r'^ {2}(\S+)  SKIP:', out, re.M))
    if n['fail'] > 30 and fails:
        return n, None, None            # 失败清单被截断
    return n, fails, skips


def attribute(batch, cc, workers, mutating, depth=0):
    """返回 {anchor: status}。mutating ⇹ RED / STILL-GREEN；baseline ⇹ PASS / FAIL / SKIP。"""
    if not batch:
        return {}
    saved = apply_batch(batch) if mutating else []
    try:
        rc, n, out = run_only(batch, cc, workers, mutating=mutating)
    finally:
        if mutating:
            bad = restore_batch(saved)
            if bad:
                raise Crash('restore failed: %s' % bad)
    if crash_evidence(out):
        raise Crash('compiler crash at batch %s' % batch[:2])
    parsed = parse_status(out, batch)
    if parsed is None:
        raise Crash('无法从 runner 输出读到 PASS=/SKIP=/FAIL= (rc=%d)' % rc)
    n, fails, skips = parsed
    if mutating:
        if n['fail'] == len(batch) and n['pass'] == 0:
            return {r: 'RED' for r in batch}
        if fails is not None:
            res = {}
            for r in batch:
                if r in fails:
                    res[r] = 'RED'
                elif r in skips:
                    res[r] = 'SKIP'
                else:
                    res[r] = 'STILL-GREEN'
            return res
    else:
        if n['pass'] == len(batch) and n['fail'] == 0:
            return {r: 'PASS' for r in batch}
        if fails is not None:
            res = {}
            for r in batch:
                if r in fails:
                    res[r] = 'FAIL'
                elif r in skips:
                    res[r] = 'SKIP'
                else:
                    res[r] = 'PASS'
            return res
    if len(batch) == 1:
        r = batch[0]
        if mutating:
            return {r: 'RED' if n['fail'] == 1 and rc != 0 else 'STILL-GREEN'}
        return {r: ('PASS' if n['pass'] == 1 else ('SKIP' if n['skip'] == 1 else 'FAIL'))}
    mid = len(batch) // 2
    a = attribute(batch[:mid], cc, workers, mutating, depth + 1)
    b = attribute(batch[mid:], cc, workers, mutating, depth + 1)
    a.update(b)
    return a


def save_state(path, rec, anchors):
    if not path:
        return
    tmp = path + '.tmp'
    io.open(tmp, 'w', encoding='utf-8', newline='\n').write(
        json.dumps({'records': [rec[r] for r in anchors]}, ensure_ascii=False, indent=1) + '\n')
    os.replace(tmp, path)


def load_state(path):
    if not path or not os.path.exists(path):
        return {}
    try:
        d = json.load(io.open(path, encoding='utf-8'))
    except Exception:
        return {}
    return {x['anchor']: x for x in d.get('records', [])}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    ap.add_argument('--mutate', type=int, default=0, help='只抽样 K 个（调试用；0 = 全量审计）')
    ap.add_argument('--seed', type=int, default=828)
    ap.add_argument('--batch', type=int, default=500, help='基线批大小')
    ap.add_argument('--mbatch', type=int, default=300, help='变异批大小')
    # 并行上限 4：Agents.md 铁律（不得并行跑全量）+ runner 的 `--only` 路径
    # 不走内存预检（run_tests_zh.py:4025），超 4 路会绕过内存护栏，故硬限 4。
    ap.add_argument('--workers', type=int, default=4)
    ap.add_argument('--log', default='')
    ap.add_argument('--json', default='')
    ap.add_argument('--focus-file', default='', help='这些锚（每行一个）必须出现在证据里且为 RED')
    ap.add_argument('--state', default='', help='实跑状态文件（JSON）：存在则续跑，每批结束就落盘')
    ap.add_argument('--budget-sec', type=int, default=3300,
                    help='本次运行时间预算（秒），到点就停并保存状态（退码 3 = 未完成）')
    a = ap.parse_args()
    t0 = time.time()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s（审计无法执行，不判绿）' % cc)
        return 2

    anchors = core_feat()
    missing = [r for r in anchors if not os.path.exists(abspath(r))]
    anchors = [r for r in anchors if os.path.exists(abspath(r))]
    exp = {r: expectation(abspath(r)) for r in anchors}
    known_gap = [r for r in anchors
                 if '@KNOWN-GAP' in io.open(abspath(r), encoding='utf-8', errors='replace').read()]
    focus = []
    if a.focus_file:
        focus = [l.strip().lstrip('\ufeff') for l in
                 io.open(a.focus_file, encoding='utf-8-sig').read().split('\n')
                 if l.strip() and not l.lstrip('\ufeff').startswith('#')]

    prev = load_state(a.state)
    rec = {}
    for r in anchors:
        kind = classify(r, anchors, exp)
        old = prev.get(r) or {}
        rec[r] = {'anchor': r, 'kind': kind,
                  'baseline': old.get('baseline', 'UNRUN'),
                  'mutation': old.get('mutation', 'UNRUN'),
                  'form': old.get('form', ''),
                  'runs': list(old.get('runs') or [])}
    kg = set(known_gap)
    todo = [r for r in anchors if r not in kg]
    if a.mutate > 0:
        random.seed(a.seed)
        todo = random.sample([r for r in todo if rec[r]['kind'] == 'content'], min(a.mutate, len(todo)))

    partial = False
    aborted = None
    try:
        # 1) 基线
        for i in range(0, len(todo), a.batch):
            batch = [r for r in todo[i:i + a.batch] if rec[r]['baseline'] == 'UNRUN']
            if not batch:
                continue
            if time.time() - t0 > a.budget_sec:
                partial = True
                break
            res = attribute(batch, cc, a.workers, mutating=False)
            for r, st in res.items():
                rec[r]['baseline'] = st
                rec[r]['runs'].append('baseline')
            save_state(a.state, rec, anchors)
            print('[baseline] %d/%d · PASS=%d FAIL/SKIP=%d · %ds'
                  % (min(i + len(batch), len(todo)), len(todo),
                     sum(1 for r in batch if rec[r]['baseline'] == 'PASS'),
                     sum(1 for r in batch if rec[r]['baseline'] in ('FAIL', 'SKIP')),
                     int(time.time() - t0)), flush=True)
        # 基线本身就红的锚：不是「为绿而绿」，也无需变异——它们已经能红（可证伪），
        # 但必须当作**新发现的红锚**登记到台账，不能熔进「已证」里。
        baseline_red = [r for r in todo if rec[r]['baseline'] == 'FAIL']
        for r in baseline_red:
            rec[r]['mutation'] = 'BASELINE-RED'
            rec[r]['form'] = 'baseline-fail'
            rec[r]['runs'].append('baseline-red')
        if not partial:
            green = [r for r in todo if rec[r]['baseline'] == 'PASS']
            # 2) 变异
            for i in range(0, len(green), a.mbatch):
                batch = [r for r in green[i:i + a.mbatch] if rec[r]['mutation'] == 'UNRUN']
                if not batch:
                    continue
                if time.time() - t0 > a.budget_sec:
                    partial = True
                    break
                res = attribute(batch, cc, a.workers, mutating=True)
                for r, st in res.items():
                    rec[r]['mutation'] = st
                    rec[r]['runs'].append('mutated')
                    if st == 'RED':
                        rec[r]['form'] = plan_mutation(
                            io.open(abspath(r), encoding='utf-8', errors='replace').read())[1]
                save_state(a.state, rec, anchors)
                print('[mutation] %d/%d · RED=%d STILL-GREEN=%d · %ds'
                      % (min(i + len(batch), len(green)), len(green),
                         sum(1 for r in batch if rec[r]['mutation'] == 'RED'),
                         sum(1 for r in batch if rec[r]['mutation'] == 'STILL-GREEN'),
                         int(time.time() - t0)), flush=True)
    except Crash as e:
        aborted = str(e)

    # known-gap：单跑基线，记录它「确实红」（本来就必须红，不需要变异）
    for r in known_gap:
        if rec[r]['baseline'] == 'UNRUN' and time.time() - t0 <= a.budget_sec:
            try:
                res = attribute([r], cc, 1, mutating=False)
                rec[r]['baseline'] = res[r]
            except Crash:
                rec[r]['baseline'] = 'UNRUN'
            rec[r]['mutation'] = 'KNOWN-GAP'
            rec[r]['runs'].append('KG:baseline')

    counted = todo if a.mutate > 0 else [r for r in anchors if r not in kg]
    proven = ('RED', 'BASELINE-RED', 'KNOWN-GAP')
    unproven = [r for r in counted if rec[r]['mutation'] not in proven]
    focus_missing = [f for f in focus if f not in rec]
    focus_not_red = [f for f in focus if f in rec and rec[f]['mutation'] != 'RED']

    L = ['== anchor_sensitivity (audit) ==',
         'anchors: %d (missing: %d)  known-gap: %d  counted: %d  batch=%d/%d workers=%d'
         % (len(anchors), len(missing), len(known_gap), len(counted), a.batch, a.mbatch, a.workers),
         'missing: %s' % (', '.join(missing) or '(none)')]
    for kind in ('content', 'negative', 'pair', 'static-only'):
        L.append('kind %-11s %d' % (kind, sum(1 for r in anchors if rec[r]['kind'] == kind)))
    L.append('baseline PASS: %d  baseline FAIL: %d  baseline SKIP: %d'
             % (sum(1 for r in anchors if rec[r]['baseline'] == 'PASS'),
                sum(1 for r in anchors if rec[r]['baseline'] == 'FAIL'),
                sum(1 for r in anchors if rec[r]['baseline'] == 'SKIP')))
    L.append('mutation RED: %d  STILL-GREEN: %d  KNOWN-GAP: %d  UNRUN: %d'
             % (sum(1 for r in anchors if rec[r]['mutation'] == 'RED'),
                sum(1 for r in anchors if rec[r]['mutation'] == 'STILL-GREEN'),
                sum(1 for r in anchors if rec[r]['mutation'] == 'KNOWN-GAP'),
                sum(1 for r in anchors if rec[r]['mutation'] == 'UNRUN')))
    L.append('baseline-red（本就红：已可证伪，已登记台账）: %d %s'
             % (len(baseline_red), baseline_red))
    L.append('unproven: %d %s' % (len(unproven), unproven[:20]))
    for r in anchors:
        L.append('  %-11s base=%-5s mut=%-11s form=%-22s %s'
                 % (rec[r]['kind'], rec[r]['baseline'], rec[r]['mutation'], rec[r]['form'], r))
    if focus:
        L.append('-- focus (%d) --' % len(focus))
        for f in focus:
            L.append('  %-11s %s' % (rec[f]['mutation'] if f in rec else 'MISSING', f))
        L.append('focus missing: %s  focus not RED: %s'
                 % (focus_missing or '(none)', focus_not_red or '(none)'))
    if aborted:
        L.append('ABORT: %s' % aborted)
    if partial:
        L.append('PARTIAL: 时间预算到点（%ds），已存状态，重跑即续' % a.budget_sec)
    text = '\n'.join(L)
    print(text[-2500:])
    if a.log:
        io.open(a.log, 'w', encoding='utf-8', newline='\n').write(text + '\n')
    if a.json:
        io.open(a.json, 'w', encoding='utf-8', newline='\n').write(
            json.dumps({'anchors': len(anchors), 'known_gap': known_gap,
                        'partial': partial, 'aborted': aborted,
                        'records': [rec[r] for r in anchors]}, ensure_ascii=False, indent=1) + '\n')
    save_state(a.state, rec, anchors)
    complete = (not missing) and (not unproven) and (not focus_missing) and (not focus_not_red) \
        and (not aborted) and (not partial)
    print('=== 锚灵敏度: %s（counted=%d · unproven=%d · partial=%s · aborted=%s）==='
          % ('PASS' if complete else ('PARTIAL' if partial and not aborted else 'FAIL'),
             len(counted), len(unproven), partial, bool(aborted)))
    return 0 if complete else (3 if partial and not aborted and not unproven else 1)


if __name__ == '__main__':
    sys.exit(main())
