# -*- coding: utf-8 -*-
"""ORD-36 修点B 复现：w6/w7 指针间接悬垂仍放行。blocked（需别名数据流，另开一轮）。

用法: python scripts/check_ord36b_ptrindirect.py [--cc qcc_zh_v1.exe]
"""
import argparse, io, os, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord36b')

CASES = {
    'w6_array_alias': ('#包含 <stdio.h>\n'
                       '字 *取(空){ 字 局[] = "x"; 字 *r = 局; 存期 a; 借用 存期 a 局; 返 r; }\n'
                       '函数 整 主(空){ 返 0; }\n'),
    'w7_ptr_alias': ('#包含 <stdio.h>\n'
                     '整 *取(空){ 整 局 = 42; 整 *r = &局; 存期 a; 借用 存期 a 局; 返 r; }\n'
                     '函数 整 主(空){ 返 0; }\n'),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(TMP, exist_ok=True)
    bad = []
    for name, src in CASES.items():
        p = os.path.join(TMP, name + '.c')
        io.open(p, 'w', encoding='utf-8').write(src)
        r = subprocess.run([cc, p, '-o', os.path.join(TMP, name + '.exe')], cwd=REPO,
                           capture_output=True, text=True, encoding='utf-8', errors='replace')
        caught = 'A3105' in (r.stderr or '')
        print('%-18s rc=%d A3105=%s' % (name, r.returncode, caught))
        if not caught:
            bad.append(name)
    if bad:
        print('[REPRO] 指针间接悬垂仍放行 %d 例（%s）⟹ blocked 诚实' % (len(bad), ', '.join(bad)))
        return 1
    print('[PASS] w6/w7 均被 A3105 拦截 ⟹ 应转 fixed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
