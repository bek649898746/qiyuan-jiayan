#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_anchor_strict —— 锚的**严格**核对（逐字比对 @EXPECTED），用于 ORD-20261031-78/79。

为什么需要它：`dev_direct_run.py` 对 `out` 是宽匹配（只看注解里的首行），曾把「打印了错值」
判成 PASS（实测 `feat_zh_ptrptr_index_ok` 输出 `B=30,%` 仍判 PASS）。本判据要求
`运行输出` 以 `// @EXPECTED out:` 的文本**逐字开头**、退出码等于 `// @EXPECTED exit:`，
`compile_fail` 锚则要求编译非零退出。

用法: python scripts/check_anchor_strict.py --cc qcc_zh_v1.exe [锚名 ...]
退出码: 0 = 全部符合；1 = 有不符合；2 = 环境未就绪。
"""
from qcc_tmp import tmp_dir, tmp_root
import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DEFAULT = ['feat_contract_zh.c', 'feat_zh_ptrptr_index_ok.c', 'feat_c_char_arr_deref_ok.c',
           'feat_c_triple_ptr_ok.c', 'feat_zh_do_while_voidptr_ok.c',
           'feat_zh_ptrvar_sub_elem_ok.c', 'feat_zh_contract_static_neg.c',
           'feat_zh_deref_idx_ok.c']


def expects(path):
    txt = io.open(path, encoding='utf-8', errors='replace').read()
    out = re.search(r'//\s*@EXPECTED\s+out:(.*)', txt)
    rc = re.search(r'//\s*@EXPECTED\s+exit:(\d+)', txt)
    cf = '@EXPECTED compile_fail' in txt
    return (out.group(1).strip() if out else None,
            int(rc.group(1)) if rc else 0,
            cf)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('anchors', nargs='*', default=DEFAULT)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s' % cc)
        return 2
    tmp = tmp_dir('anchstrict')
    bad = 0
    for name in a.anchors:
        p = os.path.join(REPO, 'scripts', name) if not os.path.isabs(name) else name
        if not os.path.exists(p):
            print('%-38s FAIL 锚文件不存在' % name)
            bad += 1
            continue
        exp_out, exp_rc, compile_fail = expects(p)
        exe = os.path.join(tmp, os.path.basename(name) + '.exe')
        r = subprocess.run([cc, p, '-o', exe], capture_output=True)
        if compile_fail:
            ok = r.returncode != 0
            print('%-38s %s (compile_fail: rc=%d)' % (name, 'PASS' if ok else 'FAIL', r.returncode))
            bad += 0 if ok else 1
            continue
        if r.returncode != 0:
            print('%-38s FAIL 编译 rc=%d' % (name, r.returncode))
            bad += 1
            continue
        rr = subprocess.run([exe], capture_output=True)
        got = (rr.stdout + rr.stderr).decode('utf-8', 'replace').replace('\r\n', '\n').strip()
        ok = (rr.returncode == exp_rc) and (exp_out is None or got.startswith(exp_out.strip()))
        print('%-38s %s rc=%d/%d out=%r%s'
              % (name, 'PASS' if ok else 'FAIL', rr.returncode, exp_rc, got[:70],
                 '' if ok or exp_out is None else ' 期望=%r' % exp_out[:70]))
        bad += 0 if ok else 1
    print('=== 锚严格核对（%d 个，编译器=%s）: %s ==='
          % (len(a.anchors), os.path.basename(cc), 'PASS' if bad == 0 else 'FAIL %d 条' % bad))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
