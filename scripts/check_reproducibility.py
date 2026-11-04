# -*- coding: utf-8 -*-
"""check_reproducibility —— 可复现性判据：被逐字节内嵌的文件必须禁止行尾转换。

背景（2026-11-02 · 启元隔离验收发现）：
  干净 clone 后 build_zh.ps1 报「不动点 888F3C97 != 期望 C0367707」。
  根因 = compat_prelude.h / qcc_rt_ws.c / qcc_snprintf_mini.c 未在 .gitattributes
  标 `-text` ⟹ core.autocrlf=true 时 checkout 出 CRLF ⟹ 被前插进产物 ⟹ 不动点变。

判据（任一红即判红）：
  ① 每个「被主源前插/内嵌」的文件，git 属性 text 必须为 unset
  ② 这些文件的工作区形态必须纯 LF
  ③ .gitattributes 必须有对应显式规则（不靠全局 core.autocrlf 赌）
  ④ 主源 qcc_zh.jy 必须纯 LF

用法: python scripts/check_reproducibility.py
退出码: 0 = PASS / 1 = RED
"""
import os, re, subprocess, sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EMBEDDED = ['compat_prelude.h', 'srclib/qcc_rt.c',
            'srclib/qcc_rt_ws.c', 'srclib/qcc_snprintf_mini.c']
CORE = ['srclib_jiayan/qcc_zh.jy']

fail = 0
def red(m):
    global fail
    fail += 1
    print('[FAIL] ' + m)
def ok(m):
    print('[ OK ] ' + m)

def count_crlf(path):
    with open(path, 'rb') as f:
        b = f.read()
    return sum(1 for i in range(1, len(b)) if b[i] == 10 and b[i-1] == 13)

def git_attr(p):
    r = subprocess.run(['git', 'check-attr', 'text', '--', p],
                       cwd=ROOT, capture_output=True)
    s = r.stdout.decode('utf-8', 'replace').strip()
    if 'text: unset' in s:
        return 'unset'
    if 'text: set' in s:
        return 'set'
    return 'unspecified'

print('=' * 58)
print(' 可复现性判据 · 被内嵌文件的行尾锁定')
print('=' * 58)

for p in EMBEDDED:
    full = os.path.join(ROOT, p.replace('/', os.sep))
    if not os.path.exists(full):
        red('%s 不存在（主源引用了它，缺文件）' % p)
        continue
    a = git_attr(p)
    if a == 'unset':
        ok('%s  text=unset（-text 已锁）' % p)
    else:
        red('%s  text=%s ⟹ checkout 会被转行尾 ⟹ 产物 hash 漂移' % (p, a))
    n = count_crlf(full)
    if n == 0:
        ok('%s  工作区纯 LF' % p)
    else:
        red('%s  工作区含 %d 个 CRLF' % (p, n))

for p in CORE:
    full = os.path.join(ROOT, p.replace('/', os.sep))
    if not os.path.exists(full):
        red('%s 不存在' % p)
        continue
    n = count_crlf(full)
    if n == 0:
        ok('%s  纯 LF' % p)
    else:
        red('%s  含 %d 个 CRLF' % (p, n))

ga = os.path.join(ROOT, '.gitattributes')
if os.path.exists(ga):
    txt = open(ga, encoding='utf-8', errors='replace').read()
    for p in EMBEDDED:
        if re.search(r'^\s*' + re.escape(p) + r'\s+-text', txt, re.M):
            ok('.gitattributes 显式规则: %s -text' % p)
        else:
            red('.gitattributes 缺显式规则: %s -text' % p)
else:
    red('.gitattributes 不存在')

print('\n' + '=' * 58)
if fail == 0:
    print(' 结论: PASS —— 被内嵌文件行尾已锁定，clone 后可复现')
    sys.exit(0)
print(' 结论: RED（%d 项失败）—— 修完再谈发布' % fail)
print(' 修法: .gitattributes 补 `<路径> -text`，且工作区该文件须为 LF')
sys.exit(1)
