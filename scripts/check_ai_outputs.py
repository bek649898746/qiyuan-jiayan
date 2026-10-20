#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AI 适配接口 · **产出质量** 判据（2026-10-19 · 地基长线）。

与 `check_ai_interface.py` 的分工：
  · check_ai_interface：**开关能跑**（27 个开关 rc / 中文别名 / -graph 行号 / --repeat 可重入）
  · 本判据：**产出可用**（AI 侧真能消费的输出语义）

覆盖 8 项：
  ① `--interface`  —— JSON 合法；用户函数行号真值；参数/返回；用户结构体布局
  ② `--refs`       —— JSON 合法；调用点在**用户行**；**预置前插段内部引用不得混入**
  ③ `--annotations`—— 注解名/值原样导出
  ④ `--recorder`   —— 每行合法 JSON；含用户函数帧；pass/delta/cp 字段齐
  ⑤ `--budget`     —— JSON 合法；14 项水位，used <= cap
  ⑥ `--dag`        —— 四阶段哈希齐；artifact == 产物 SHA256
  ⑦ `--cache`      —— 同输出路径第二次命中（首次 miss → 二次 hit）
  ⑧ `-graph`       —— JSON 合法；用户函数/结构体实体行号与源码一致

用法:
  python scripts/check_ai_outputs.py --cc qcc_zh_v2.exe
退出码: 0 全过；1 有失败（逐项打印 FAIL 原因）
"""
import argparse
import hashlib
import io
import json
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROBE = '''/* AI outputs quality probe */
#include <stdio.h>
struct 学生 { int 分数; };
int 加一(int x) { return x + 1; }
注解 版本 = "1.2.3";
注解 计数 = 2;
int main(void) {
    struct 学生 s;
    s.分数 = 加一(6);
    printf("AI %d\\n", s.分数);
    return 0;
}
'''


def run(args, cwd, timeout=240):
    return subprocess.run(args, cwd=cwd, capture_output=True, timeout=timeout)


def text(raw):
    """Decode compiler output: UTF-8 first, GBK fallback (D1 contract)."""
    try:
        return raw.decode('utf-8')
    except UnicodeDecodeError:
        return raw.decode('gbk', 'replace')


def load_json(raw, tag, fails):
    t = text(raw).lstrip('\ufeff')
    try:
        return json.loads(t)
    except Exception:
        pass
    # 某些开关先打印 "OK: ..." 再输出 JSON（--budget/--dag）⟹ 从后往前取首个 JSON 行
    for ln in reversed(t.splitlines()):
        ln = ln.strip()
        if not ln.startswith('{'):
            continue
        try:
            return json.loads(ln)
        except Exception:
            continue
    fails.append('%s: 输出不是合法 JSON' % tag)
    return None


def line_of(src, needle):
    for i, ln in enumerate(src.splitlines(), 1):
        if needle in ln:
            return i
    return -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('编译器不存在: %s' % cc)
        return 2

    w = tempfile.mkdtemp(prefix='qcc_ai_out_')
    src = os.path.join(w, 'p.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(PROBE)
    nlines = len(PROBE.splitlines())
    exp_fn = line_of(PROBE, 'int 加一')
    exp_main = line_of(PROBE, 'int main')
    exp_call = line_of(PROBE, '加一(6)')      # 调用点 (引用图应记这一行)
    exp_s_use = line_of(PROBE, 's.分数 = 加一(6)')

    fails = []

    # ① --interface
    r = run([cc, '--interface', src], w)
    d = load_json(r.stdout, '--interface', fails)
    if d is not None:
        it = d.get('interface', {})
        fns = {f['name']: f for f in it.get('functions', [])}
        if '加一' not in fns:
            fails.append('--interface: 缺用户函数 加一')
        else:
            f = fns['加一']
            if f.get('line') != exp_fn:
                fails.append('--interface: 加一 line=%r 应 %d' % (f.get('line'), exp_fn))
            if f.get('ret') != 'int':
                fails.append('--interface: 加一 ret=%r 应 int' % f.get('ret'))
            ps = f.get('params') or []
            if len(ps) != 1 or ps[0].get('name') != 'x' or ps[0].get('type') != 'int':
                fails.append('--interface: 加一 参数=%r 应 [{x:int}]' % (ps,))
        if 'main' not in fns:
            fails.append('--interface: 缺 main')
        elif fns['main'].get('line') != exp_main:
            fails.append('--interface: main line=%r 应 %d' % (fns['main'].get('line'), exp_main))
        st = {s['name']: s for s in it.get('structs', [])}
        if '学生' not in st:
            fails.append('--interface: 缺用户结构体 学生')
        else:
            s = st['学生']
            if s.get('size') != 4:
                fails.append('--interface: 学生 size=%r 应 4' % (s.get('size'),))
            fs = s.get('fields') or []
            if len(fs) != 1 or fs[0].get('name') != '分数' or fs[0].get('off') != 0:
                fails.append('--interface: 学生 字段=%r 应 [{分数,off:0}]' % (fs,))

    # ② --refs
    r = run([cc, '--refs', src], w)
    d = load_json(r.stdout, '--refs', fails)
    if d is not None:
        refs = {x['name']: x.get('sites', []) for x in d.get('refs', [])}
        if refs.get('加一') != [exp_call]:
            fails.append('--refs: 加一 sites=%r 应 [%d]' % (refs.get('加一'), exp_call))
        if exp_s_use not in refs.get('s', []):
            fails.append('--refs: 缺用户变量 s 的调用行 %d (实际 %r)' % (exp_s_use, refs.get('s')))
        # 预置段污染: 这些名字只在运行库内部出现, 不应出现在用户引用图
        for bad in ('whence', '_setpos', '_getpos'):
            if bad in refs:
                fails.append('--refs: 预置段内部符号 %r 混入用户引用图 (sites=%r)' % (bad, refs[bad]))
        for name, sites in refs.items():
            for s in sites:
                if not (1 <= s <= nlines):
                    fails.append('--refs: %r 站点 %d 越出用户行范围 1..%d' % (name, s, nlines))
                    break

    # ③ --annotations
    r = run([cc, '--annotations', src], w)
    t = text(r.stdout) + text(r.stderr)
    for want in ('注解: 版本 = 1.2.3', '注解: 计数 = 2'):
        if want not in t:
            fails.append('--annotations: 缺 %r' % want)

    # ④ --recorder
    rec = os.path.join(w, 'rec.jsonl')
    out = os.path.join(w, 'o.exe')
    r = run([cc, '--recorder', rec, src, '-o', out], w)
    if not os.path.exists(rec):
        fails.append('--recorder: 文件未生成')
    else:
        seen = set()
        for i, ln in enumerate(io.open(rec, encoding='utf-8', errors='replace'), 1):
            ln = ln.strip()
            if not ln:
                continue
            try:
                fr = json.loads(ln)
            except Exception:
                fails.append('--recorder: 第 %d 行不是 JSON' % i)
                break
            if 'pass' not in fr:
                fails.append('--recorder: 第 %d 行缺字段 pass' % i)
                break
            if fr.get('start'):
                continue  # 每遍起始帧: {"pass":N,"start":true}
            for k in ('fn', 'delta', 'cp'):
                if k not in fr:
                    fails.append('--recorder: 第 %d 行缺字段 %r' % (i, k))
                    break
            seen.add(fr.get('fn'))
        for fn in ('加一', 'main'):
            if fn not in seen:
                fails.append('--recorder: 缺用户函数帧 %r' % fn)

    # ⑤ --budget
    r = run([cc, '--budget', src, '-o', out], w)
    d = load_json(r.stdout, '--budget', fails)
    if d is not None:
        b = d.get('budget')
        if not isinstance(b, list) or len(b) < 14:
            fails.append('--budget: 水位项 %r 应 >=14' % (len(b) if isinstance(b, list) else b,))
        else:
            for item in b:
                if item.get('used', 0) > item.get('cap', 0):
                    fails.append('--budget: %s used>cap' % item.get('name'))

    # ⑥ --dag
    r = run([cc, '--dag', src, '-o', out], w)
    d = load_json(r.stdout, '--dag', fails)
    if d is not None:
        st = d.get('dag', {}).get('stages', {})
        for k in ('source', 'tokens', 'ast', 'artifact'):
            if len(str(st.get(k, ''))) != 64:
                fails.append('--dag: 阶段 %r 哈希长度不是 64' % k)
        if os.path.exists(out):
            h = hashlib.sha256(io.open(out, 'rb').read()).hexdigest()
            if st.get('artifact') != h:
                fails.append('--dag: artifact != 产物 SHA256')

    # ⑦ --cache (same output path twice)
    cache_out = os.path.join(w, 'c.exe')
    r1 = run([cc, '--cache', src, '-o', cache_out], w)
    r2 = run([cc, '--cache', src, '-o', cache_out], w)
    t1, t2 = text(r1.stdout) + text(r1.stderr), text(r2.stdout) + text(r2.stderr)
    if 'miss' not in t1:
        fails.append('--cache: 首次应 miss (实际 %r)' % t1[-80:])
    if 'hit' not in t2 or 'cached' not in t2:
        fails.append('--cache: 二次同路径应 hit/cached (实际 %r)' % t2[-80:])

    # ⑧ -graph
    gp = os.path.join(w, 'g.json')
    r = run([cc, '-graph', gp, src, '-o', out], w)
    if not os.path.exists(gp):
        fails.append('-graph: 文件未生成')
    else:
        try:
            g = json.load(io.open(gp, encoding='utf-8-sig', errors='replace'))
        except Exception as e:
            fails.append('-graph: 不是合法 JSON (%s)' % e)
            g = None
        if g is not None:
            ents = g.get('entities') or g.get('nodes') or []
            if isinstance(ents, list):
                by = {}
                for e in ents:
                    if isinstance(e, dict):
                        by[e.get('id', '')] = e
                        by.setdefault(e.get('name', ''), e)
                for name, exp in (('加一', exp_fn), ('main', exp_main)):
                    k = 'fn:' + name
                    if k not in by:
                        fails.append('-graph: 缺实体 %s' % k)
                    elif by[k].get('line') != exp:
                        fails.append('-graph: %s line=%r 应 %d' % (k, by[k].get('line'), exp))

    if fails:
        print('=== AI outputs: FAIL (%d) ===' % len(fails))
        for f in fails:
            print('  - ' + f)
        print('workdir:', w)
        return 1
    print('=== AI outputs: PASS (8/8) ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
