#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_a9006_cast_const —— A9006「显式转型剥 const 后仍被拦」回归探针（2026-10-17 挂账）。

背景（`_DEBT_INDEX.md` 2026-10-17 段 + `probes/a9006_cast_strip_const/README.md`）:
  `parse-options.c:927`  `*(char *)ctx->argv[0] = '-';`
  `struct parse_opt_ctx_t { const char **argv; }` —— 显式转型剥 const 后写，C 合法（gcc 接受），
  甲言 `指只读写拦()` 仍按字段级 `指只读` 拦下 ⟹ 假 A9006。

本脚本内嵌四个最小形态（不依赖 `probes/` 目录，因该目录在 .gitignore）:
  p1 字段 + 显式转型            期望（修好后）rc=0   —— 当前 rc=1（缺陷）
  p2 局部 const + 显式转型       期望（修好后）rc=0   —— 当前 rc=1（缺陷）
  p3 字段 + **无**转型           期望 rc=1（C 约束违反，正确拦截）
  p4 形参 + 显式转型            期望 rc=0（当前已绿 = 两载体口径不一致的证据）

用法:
  python scripts/check_a9006_cast_const.py [--cc <exe>] [--expect-bug]
默认按「修好后」判定（p1/p2/p4 应绿、p3 应红）；`--expect-bug` 按当前挂账状态判定，
仅用于确认缺陷仍可复现（不会因失败而返回非零以外的副作用）。
"""
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
CC = 'qcc_zh_v2.exe'
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
EXPECT_BUG = '--expect-bug' in sys.argv
TMP = os.path.join(os.environ.get('TEMP', '.'), '_check_a9006')
PRE = os.path.join(REPO, 'compat_prelude.h')

CASES = {
    'p1_cast_field': ('struct C { const char **argv; };\n'
                      'void f(struct C *c){ *(char *)c->argv[0] = \'-\'; }\n', 0),
    'p2_cast_local': ('void f(void){ const char *s = "x"; *(char *)s = \'y\'; }\n', 0),
    'p3_no_cast': ('struct C { const char **argv; };\n'
                   'void f(struct C *c){ *(c->argv[0]) = \'-\'; }\n', 1),
    'p4_param_cast': ('void f(const char **argv){ *(char *)argv[0] = \'-\'; }\n', 0),
}
# 挂账状态下（--expect-bug）的期望 rc
BUG_EXPECT = {'p1_cast_field': 1, 'p2_cast_local': 1, 'p3_no_cast': 1, 'p4_param_cast': 0}


def main():
    os.makedirs(TMP, exist_ok=True)
    fails = []
    for name, (src, want_ok_rc) in CASES.items():
        p = os.path.join(TMP, name + '.c')
        open(p, 'w', encoding='utf-8').write(src)
        r = subprocess.run([CC, '-I', PRE, '-c', p, '-o', os.path.join(TMP, name + '.o')],
                           capture_output=True, timeout=120)
        want = BUG_EXPECT[name] if EXPECT_BUG else want_ok_rc
        ok = (r.returncode == want)
        msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip().replace('\n', ' | ')[:110]
        print('%-16s rc=%d want=%d %s  %s' % (name, r.returncode, want, 'OK' if ok else 'MISMATCH', msg))
        if not ok:
            fails.append(name)
    if EXPECT_BUG:
        ok = all(not f for f in fails)
        print('=== 挂账状态确认: %s ===' % ('PASS（缺陷仍可复现）' if ok else 'FAIL'))
    else:
        ok = not fails
        print('=== 目标状态判定: %s ===' % ('PASS（转型剥 const 已放行，无转型仍拦）' if ok else 'FAIL（缺陷仍在：' + ','.join(fails) + '）'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
