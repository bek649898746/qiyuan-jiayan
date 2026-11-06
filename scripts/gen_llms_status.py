#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""gen_llms_status —— `llms.txt` / `llms-full.txt` 的机器事实块生成器 + 闸。

由来（ORD-20261105-126 ③④）
--------------------------
对外单页 `llms.txt` / `llms-full.txt` 里的「主源多少行 / 不动点 / 多语言规模」
全靠人肉抄写。施工线约 1.5 提交/小时 ⟹ 抄下来的数字必然漂；
而 `.github/workflows/status-sync.yml` 当年只把 `gen_readme_status.ps1`
接进自动同步，`llms*.txt` 被漏在范围外 ⟹ 漂了也没人管。

根治法（不是再补一次数字）
------------------------
1. 会漂的数字**只允许**出现在标记块里：

       <!-- GEN:LLMS:FACTS:BEGIN ... -->
       ...
       <!-- GEN:LLMS:FACTS:END -->

2. 块内容由本脚本从「**同一个 ref 自己的树**」现算 —— 默认工作区，
   `--ref <ref>` 则用 `git show <ref>:<path>` 读，所以发布分支也能判。
3. 块外再出现同类数字（`N,NNN 行` / `85 文件 / 50,692 行`）⟹ 判红，
   挡住「人肉又抄一份」这条回归路径。
4. `--check` 只比对不写盘，供 `scripts/check_failopen_negatives.py` 的 G/H 组使用；
   `--write` 供 `status-sync.yml` 在发布分支上自动刷新。

用法
----
    python scripts/gen_llms_status.py                 # 只看状态（= --check）
    python scripts/gen_llms_status.py --write          # 就地刷新块
    python scripts/gen_llms_status.py --check --ref publish
    python scripts/gen_llms_status.py --check --json   # 机器可读

退出码：0 = 块存在且与现算一致；1 = 缺失 / 过期 / 块外重复数字；2 = 用法错。
"""
import argparse
import json
import os
import re
import subprocess
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BEGIN = '<!-- GEN:LLMS:FACTS:BEGIN'
END = '<!-- GEN:LLMS:FACTS:END -->'
TARGETS = ('llms.txt', 'llms-full.txt')

# 块外禁写规则（两种，互补）：
#   A. 精确重复 —— 块外又抄了一遍生成器已算出的那个数字（哪怕抄对了也算重复）；
#   B. 语境另写 —— 提到主源 / 多语言源码时又写一套行数（陈旧数字走这条）。
# 只拦「会漂的行数类事实」；机制数 / 关键字数等口径表述不在此列。
SRC_HINT = 'qcc_zh.jy'
ML_HINT = 'multilang'


# --------------------------------------------------------------------------- #
# 取数层：工作区 或 任意 git ref
# --------------------------------------------------------------------------- #
class Tree(object):
    def __init__(self, ref=None):
        self.ref = ref

    def read(self, rel):
        """返回 bytes；不存在返回 None。"""
        rel = rel.replace('\\', '/')
        if self.ref:
            p = subprocess.run(['git', 'show', '%s:%s' % (self.ref, rel)],
                               cwd=ROOT, capture_output=True)
            return p.stdout if p.returncode == 0 else None
        path = os.path.join(ROOT, rel)
        if not os.path.isfile(path):
            return None
        with open(path, 'rb') as f:
            return f.read()

    def list(self, prefix, suffix=''):
        """列出以 prefix 开头、以 suffix 结尾的文件（相对路径，/ 分隔）。"""
        if self.ref:
            p = subprocess.run(['git', 'ls-tree', '-r', '--name-only', self.ref],
                               cwd=ROOT, capture_output=True)
            out = p.stdout.decode('utf-8', 'surrogateescape').splitlines()
        else:
            out = []
            for dirpath, _dirs, names in os.walk(os.path.join(ROOT, prefix.split('/')[0])):
                for n in names:
                    full = os.path.join(dirpath, n)
                    out.append(os.path.relpath(full, ROOT).replace('\\', '/'))
        return sorted(x for x in out if x.startswith(prefix) and x.endswith(suffix))

    def text(self, rel):
        b = self.read(rel)
        return None if b is None else _decode(b)


def _decode(b):
    """统一成 LF 再比对 —— Windows 检出把 LF 换成 CRLF 时，块比对不能因此假红。"""
    return b.decode('utf-8', 'surrogateescape').replace('\r\n', '\n').replace('\r', '\n')


def _lines(b):
    """行数（与编辑器「行」口径一致：结尾无换行的最后一行也算一行）。"""
    return len(_decode(b).splitlines())


def collect(tree):
    """从给定树现算事实。缺文件 ⟹ 对应项为 None（绝不猜）。"""
    facts = {}

    src = tree.read('srclib_jiayan/qcc_zh.jy')
    facts['src_lines'] = _lines(src) if src is not None else None

    build = tree.text('scripts/build_zh.ps1')
    fp = None
    if build:
        m = re.search(r"^\$expected\s*=\s*'([0-9A-Fa-f]{8})'", build, re.M)
        if m:
            fp = m.group(1).upper()
    facts['fixed_point'] = fp

    ml = tree.list('examples/multilang/', '.c')
    if ml:
        total = 0
        for rel in ml:
            b = tree.read(rel)
            if b is not None:
                total += _lines(b)
        facts['multilang_files'] = len(ml)
        facts['multilang_lines'] = total
    else:
        facts['multilang_files'] = None
        facts['multilang_lines'] = None

    return facts


# --------------------------------------------------------------------------- #
# 渲染 / 校验
# --------------------------------------------------------------------------- #
def render_block(facts):
    rows = []
    if facts['src_lines'] is not None:
        rows.append('- 主源 `srclib_jiayan/qcc_zh.jy` · **%s 行**（全中文标识符）'
                    % format(facts['src_lines'], ','))
    if facts['fixed_point']:
        rows.append('- 自举不动点 · **%s**（权威源 = `scripts/build_zh.ps1` 的 `$expected`）'
                    % facts['fixed_point'])
    if facts['multilang_files']:
        rows.append('- 多语言源码 · `examples/multilang/` · **%s 文件 / %s 行**'
                    % (format(facts['multilang_files'], ','), format(facts['multilang_lines'], ',')))
    body = '\n'.join(rows)
    return (BEGIN + ' · 由 scripts/gen_llms_status.py 从本 ref 的树现算 · 请勿手改 -->\n'
            + body + '\n' + END)


BLOCK_RE = re.compile(re.escape(BEGIN) + r'.*?' + re.escape(END), re.S)


def dup_problems(name, outside, facts):
    """块外重复检测（outside 已把事实块换成占位符）。"""
    out = []

    def _hit(what, frag):
        out.append('%s: 事实块外仍写着 %s —— 「%s」（只允许由生成器给出）'
                   % (name, what, frag.strip()))

    # A. 精确重复
    if facts.get('src_lines') is not None:
        num = format(facts['src_lines'], ',')
        for m in re.finditer(re.escape(num) + r'\s*行', outside):
            _hit('主源行数', m.group(0))
    if facts.get('multilang_lines'):
        num = format(facts['multilang_lines'], ',')
        for m in re.finditer(re.escape(num) + r'\s*行', outside):
            _hit('多语言行数', m.group(0))
    if facts.get('fixed_point'):
        for m in re.finditer(r'\b' + facts['fixed_point'] + r'\b', outside, re.I):
            _hit('自举不动点', m.group(0))

    # B. 语境另写（陈旧数字）
    for ln in outside.splitlines():
        if SRC_HINT in ln:
            m = re.search(r'\d[\d,]*\s*行', ln)
            if m:
                _hit('主源行数的另一份写法', m.group(0))
        if ML_HINT in ln:
            m = re.search(r'\d[\d,]*\s*(?:文件|行)', ln)
            if m:
                _hit('多语言规模的另一份写法', m.group(0))
    return out


def check_text(name, txt, block, facts, problems):
    """校验单个文件：块存在、内容等于现算、块外无重复数字。"""
    m = BLOCK_RE.search(txt)
    if not m:
        problems.append('%s: 缺 GEN:LLMS:FACTS 块（数字仍是人肉抄写 ⟹ 必然过期）' % name)
        return
    got = m.group(0)
    if got.strip() != block.strip():
        problems.append('%s: 事实块与现算不一致\n    --- 文件里 ---\n%s\n    --- 现算 ---\n%s'
                        % (name, got.strip(), block.strip()))
    outside = BLOCK_RE.sub('<!--GEN-BLOCK-->', txt)
    problems.extend(dup_problems(name, outside, facts))


def write_text(name, txt, block):
    if BLOCK_RE.search(txt):
        return BLOCK_RE.sub(lambda _m: block, txt, count=1), '替换'
    # 插在首个二级标题之前（标题之后、正文之前），最醒目
    m = re.search(r'^## ', txt, re.M)
    if m:
        return txt[:m.start()] + block + '\n\n' + txt[m.start():], '插入（首个 ## 之前）'
    return txt.rstrip() + '\n\n' + block + '\n', '追加'


def main():
    ap = argparse.ArgumentParser(description='llms.txt / llms-full.txt 机器事实块生成与校验')
    ap.add_argument('--write', action='store_true', help='就地刷新（默认只检查）')
    ap.add_argument('--check', action='store_true', help='只校验（默认行为）')
    ap.add_argument('--ref', default=None, help='从 git ref 取树（如 publish）；缺省用工作区')
    ap.add_argument('--json', action='store_true', help='输出机器可读结果')
    a = ap.parse_args()

    tree = Tree(a.ref)
    facts = collect(tree)
    block = render_block(facts)

    present = []
    for name in TARGETS:
        b = tree.read(name)
        if b is not None:
            present.append((name, _decode(b)))

    if not present:
        out = {'ref': a.ref or '(worktree)', 'objects': [], 'status': 'absent',
               'note': '本 ref 不发布 llms.txt / llms-full.txt（126③④ 对象不存在）',
               'facts': facts}
        print(json.dumps(out, ensure_ascii=False, indent=2) if a.json
              else '[SKIP] %s 没有 llms.txt / llms-full.txt ⟹ 无对象（不是通过，是缺席）'
                   % (a.ref or '工作区'))
        return 0

    if a.ref:
        # ref 模式只读，不能写
        a.write = False

    problems = []
    for name, txt in present:
        if a.write:
            new, mode = write_text(name, txt, block)
            if new != txt:
                path = os.path.join(ROOT, name)
                with open(path, 'w', encoding='utf-8', newline='\n') as f:
                    f.write(new)
                print('[WRITE] %s: %s (%d → %d 字节)' % (name, mode, len(txt), len(new)))
            else:
                print('[OK] %s: 已是最新' % name)
        else:
            check_text(name, txt, block, facts, problems)

    if a.write:
        return 0

    out = {'ref': a.ref or '(worktree)', 'objects': [n for n, _ in present],
           'status': 'fail' if problems else 'ok', 'facts': facts, 'problems': problems}
    if a.json:
        print(json.dumps(out, ensure_ascii=False, indent=2))
    elif problems:
        for p in problems:
            print('[FAIL] %s' % p)
        print('=== llms 事实块: FAIL（%d 处）===' % len(problems))
    else:
        for n, _ in present:
            print('[PASS] %s: 事实块与现算一致，块外无重复数字' % n)
        print('=== llms 事实块: PASS（%s）===' % ', '.join(n for n, _ in present))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
