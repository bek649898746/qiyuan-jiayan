# -*- coding: utf-8 -*-
"""check_assert_semantics —— ORD-20261104-110 `assert` 家族 **语义面** 判据（施工单 §六 2/3/4）。

只让 assert 编过毫无意义：必须验证「条件为假真的炸」「NDEBUG 真的静默」
「_Static_assert(假) 真的编译期拒绝」。本器三路独立编译/运行判。

用法: python scripts/check_assert_semantics.py [编译器]   # 默认 qcc_zh_v1.exe
退出码: 0 = 全绿; 1 = 有红
"""
import sys as _sys
for _s in (getattr(_sys, 'stdout', None), getattr(_sys, 'stderr', None)):
    if _s is not None and hasattr(_s, 'reconfigure'):
        _s.reconfigure(encoding='utf-8', errors='replace')
from qcc_tmp import tmp_dir, tmp_root
import io, os, subprocess, sys

JY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _pick_cc(argv):
    """`--cc <path>`（统一口径，供 run_criteria --product 改写修前腿）或 位置参数 选编译器。

    2026-11-05（虾米 · 第七轮）：本器原只认位置参数，而判据链的「修前腿」只改写 `--cc`
    ⟹ `run_criteria --product <旧编译器>` 对本行**静默失效**（旧编译器没被用上，before 腿假绿）。
    两种写法都接受，位置参数保持向后兼容。
    """
    cc, rest, i = None, [], 0
    while i < len(argv):
        a = argv[i]
        if a == '--cc' and i + 1 < len(argv):
            cc = argv[i + 1]; i += 2; continue
        if a.startswith('--cc='):
            cc = a.split('=', 1)[1]; i += 1; continue
        rest.append(a); i += 1
    if cc is None and rest:
        cc = rest[0]
    return cc


_picked = _pick_cc(sys.argv[1:])
CC = _picked if _picked else os.path.join(JY, 'qcc_zh_v1.exe')
if not os.path.exists(CC):
    print('[SKIP] 环境未就绪: 找不到 编译器 %s —— 未执行本闸' % CC)
    sys.exit(0)

TMP = os.path.join(tmp_root(), '_assert_sem')
os.makedirs(TMP, exist_ok=True)

SRC = {
 'ok': '#include <assert.h>\nint main(void){ assert(1); return 0; }\n',
 'fail': '#include <assert.h>\nint main(void){ assert(0); return 0; }\n',
 'ndebug': '#define NDEBUG\n#include <assert.h>\n#include <stdio.h>\nint main(void){ assert(0); printf("quiet\\n"); return 0; }\n',
 'sa_true': '#include <assert.h>\nint main(void){ _Static_assert(1, "x"); return 0; }\n',
 'sa_false': '#include <assert.h>\nint main(void){ _Static_assert(0, "x"); return 0; }\n',
}


def build(name):
    src = os.path.join(TMP, name + '.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC[name])
    exe = os.path.join(TMP, name + '.exe')
    if os.path.exists(exe):
        os.remove(exe)
    r = subprocess.run([CC, src, '-o', exe], capture_output=True, encoding='utf-8', errors='replace', timeout=300)
    return r, exe


def main():
    bad = 0
    r, exe = build('ok')
    if r.returncode != 0: print('  ❌ assert(1) 编译失败: %s' % (r.stderr or r.stdout)[:120]); bad += 1
    else:
        q = subprocess.run([exe], capture_output=True, encoding='utf-8', errors='replace', timeout=60)
        if q.returncode != 0: print('  ❌ assert(1) 运行 rc=%d' % q.returncode); bad += 1
        else: print('  ✓ assert(1) 编译+运行 rc=0')

    r, exe = build('fail')
    if r.returncode != 0: print('  ❌ assert(0) 编译失败'); bad += 1
    else:
        q = subprocess.run([exe], capture_output=True, encoding='utf-8', errors='replace', timeout=60)
        out = (q.stdout or '') + (q.stderr or '')
        if q.returncode == 0: print('  ❌ assert(0) 运行 rc=0（假条件未触发）'); bad += 1
        elif 'assert failed' not in out: print('  ❌ assert(0) 触发但无位置信息: %r' % out[:100]); bad += 1
        else: print('  ✓ assert(0) 触发 rc=%d %r' % (q.returncode, out.strip()[:70]))

    r, exe = build('ndebug')
    if r.returncode != 0: print('  ❌ NDEBUG 编译失败'); bad += 1
    else:
        q = subprocess.run([exe], capture_output=True, encoding='utf-8', errors='replace', timeout=60)
        out = (q.stdout or '') + (q.stderr or '')
        if q.returncode != 0: print('  ❌ NDEBUG 下 assert(0) 仍触发 rc=%d' % q.returncode); bad += 1
        elif 'assert failed' in out: print('  ❌ NDEBUG 下仍打印断言信息'); bad += 1
        elif 'quiet' not in out: print('  ❌ NDEBUG 下输出异常: %r' % out[:100]); bad += 1
        else: print('  ✓ NDEBUG 静默 rc=0')

    r, exe = build('sa_true')
    if r.returncode != 0: print('  ❌ _Static_assert(1) 编译失败'); bad += 1
    else: print('  ✓ _Static_assert(1) 通过')

    r, exe = build('sa_false')
    out = (r.stdout or '') + (r.stderr or '')
    if r.returncode == 0: print('  ❌ _Static_assert(0) 竟然通过（应编译期拒绝）'); bad += 1
    elif 'A6017' not in out: print('  ❌ _Static_assert(0) 报错但非 A6017: %r' % out[:100]); bad += 1
    else: print('  ✓ _Static_assert(0) A6017 拒绝')

    print('  结论: %s' % ('全绿' if bad == 0 else '%d 项红' % bad))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
