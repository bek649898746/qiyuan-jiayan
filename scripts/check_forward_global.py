# -*- coding: utf-8 -*-
"""R610 判据 v5（适配 v8 按需预登记）：
  正例：声明在前 41 / 显式 外部 42 / 后置 `整` 41 / 后置 `长整` 50
  负例（必须响亮拒绝）：前向写 / 浮点 / 数组 / 同块局部前向
"""
import argparse
import os
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_fwd')

FWD_READ = '''# 包含 <stdio.h>
整 g1 = 7;
函数 整 读(空) { 返 g2; }
整 g2 = 41;
函数 整 主(空) { 格式化 "%d\\n", 读(); 返 0; }
'''

FWD_LL = '''# 包含 <stdio.h>
函数 整 读(空) { 返 (整)(g8 / 100000000); }
长整 g8 = 5000000000;
函数 整 主(空) { 格式化 "%d\\n", 读(); 返 0; }
'''

FWD_WRITE = '''# 包含 <stdio.h>
整 g1 = 7;
空 写(空) { g2 = 5; }
整 g2 = 41;
函数 整 主(空) { 写(); 格式化 "%d\\n", g2; 返 0; }
'''

DECL_FIRST = '''# 包含 <stdio.h>
整 g1 = 7;
整 g2 = 41;
函数 整 读(空) { 返 g2; }
函数 整 主(空) { 格式化 "%d\\n", 读(); 返 0; }
'''

EXTERN_FIRST = '''# 包含 <stdio.h>
外部 整 戊;
函数 整 取(空) { 返 戊 + 1; }
整 戊 = 41;
函数 整 主(空) { 格式化 "%d\\n", 取(); 返 0; }
'''

FWD_DBL = '''# 包含 <stdio.h>
函数 整 读(空) { 返 (整)(gd * 2); }
双精度 gd = 41.5;
函数 整 主(空) { 格式化 "%d\\n", 读(); 返 0; }
'''

FWD_ARR = '''# 包含 <stdio.h>
函数 整 读(空) { 返 ga[0]; }
整 ga[2] = {41, 42};
函数 整 主(空) { 格式化 "%d\\n", 读(); 返 0; }
'''

FWD_LOCAL = '''# 包含 <stdio.h>
函数 整 主(空) {
    整 甲 = 乙 + 1;
    整 乙 = 5;
    格式化 "%d\\n", 甲;
    返 0;
}
'''


def run_case(cc, name, src):
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(WORK, name + '.c')
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(src)
    exe = os.path.join(WORK, name + '.exe')
    r = subprocess.run([cc, p, '-o', exe], cwd=ROOT, capture_output=True,
                       env=dict(os.environ, QCC_SMALL_HEAP='1'), timeout=300)
    txt = (r.stdout + r.stderr).decode('utf-8', 'replace')
    out = ''
    if r.returncode == 0 and os.path.exists(exe):
        e = subprocess.run([exe], capture_output=True, timeout=60)
        out = e.stdout.decode('utf-8', 'replace').strip()
    return r.returncode, txt, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 环境未就绪: %s 不存在' % cc)
        return 2
    print('# R610 判据  cc=%s' % os.path.basename(cc))
    ok = True

    for name, src, want in (('decl_first', DECL_FIRST, '41'), ('extern    ', EXTERN_FIRST, '42'),
                            ('fwd_read  ', FWD_READ, '41'), ('fwd_long  ', FWD_LL, '50')):
        rc, txt, out = run_case(cc, name.strip(), src)
        good = rc == 0 and out == want
        ok = ok and good
        print('[%s] rc=%d out=%r（期望 %s） : %s' % (name, rc, out, want, 'PASS' if good else 'FAIL'))
        if not good:
            print('    ' + txt.strip().replace('\n', ' | ')[:200])

    for name, src in (('fwd_write ', FWD_WRITE), ('fwd_double', FWD_DBL),
                      ('fwd_array ', FWD_ARR), ('fwd_local ', FWD_LOCAL)):
        rc, txt, out = run_case(cc, name.strip(), src)
        loud = ('A1001' in txt) or ('A6110' in txt)
        good = rc != 0 and loud
        ok = ok and good
        print('[%s] rc=%d 响亮=%s（必须拒） : %s' % (name, rc, loud, 'PASS' if good else 'FAIL'))
        if not good:
            print('    ' + txt.strip().replace('\n', ' | ')[:200])

    print('# 汇总: %s' % ('全部符合 R610 契约' if ok else '有不符合'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
