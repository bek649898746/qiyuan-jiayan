# -*- coding: utf-8 -*-
"""check_findqcc_refusal — 审计 P2-5 判据（2026-10-21 虾米）

病灶：`find_qcc()` 找不到 `qcc_zh_v1..v7.exe` 时**静默退化**到英文链 `qcc_x86.exe` ⟹
隔离 worktree 里「环境缺件」被误判成「中文锚全红」（英文链无中文诊断码）。
修法：拒绝退化，打印原因并 `exit(2)`。

判据（在临时目录里造一个**没有中文编译器**的最小仓布局，跑出厂的 run_tests_zh.py）：
 ① rc == **2**
 ② 输出含「拒绝退化到英文链」
 ③ 对照：`QCC_TEST_BIN=<真编译器>` 指过去 ⟹ rc=0 且 PASS=1（证明只在该拒时拒）

用法: python scripts/check_findqcc_refusal.py [--cc <exe>]
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    d = tempfile.mkdtemp(prefix='fq_')
    sd = os.path.join(d, 'scripts')
    os.makedirs(sd, exist_ok=True)
    shutil.copy(os.path.join(REPO, 'scripts', 'run_tests_zh.py'), sd)
    shutil.copy(os.path.join(REPO, 'scripts', 'feat_zh_typeof_ptr_ok.c'), sd)

    env = dict(os.environ)
    env.pop('QCC_TEST_BIN', None)
    r = subprocess.run([sys.executable, os.path.join(sd, 'run_tests_zh.py'), '--only',
                        'scripts/feat_zh_typeof_ptr_ok.c', '--parallel', '1'],
                       capture_output=True, text=True, encoding='utf-8', errors='replace',
                       cwd=d, env=env, timeout=300)
    out = (r.stdout or '') + (r.stderr or '')
    ok1 = (r.returncode == 2 and '拒绝退化到英文链' in out)
    print('① 无中文编译器: rc=%d 含拒绝=%s -> %s'
          % (r.returncode, '拒绝退化到英文链' in out, 'PASS' if ok1 else 'FAIL'))
    if not ok1:
        print('   out: %s' % out.strip()[-300:])

    env2 = dict(env)
    env2['QCC_TEST_BIN'] = os.path.abspath(a.cc)
    r2 = subprocess.run([sys.executable, os.path.join(sd, 'run_tests_zh.py'), '--only',
                         'scripts/feat_zh_typeof_ptr_ok.c', '--parallel', '1'],
                        capture_output=True, text=True, encoding='utf-8', errors='replace',
                        cwd=d, env=env2, timeout=300)
    out2 = (r2.stdout or '') + (r2.stderr or '')
    ok2 = (r2.returncode == 0 and 'PASS=1' in out2.replace(' ', ''))
    print('② QCC_TEST_BIN 指定: rc=%d PASS=1=%s -> %s'
          % (r2.returncode, 'PASS=1' in out2.replace(' ', ''), 'PASS' if ok2 else 'FAIL'))
    ok = ok1 and ok2
    print('=== AUDIT-P2-5 判据（拒绝退化到英文链）: %s ===' % ('PASS' if ok else 'FAIL'))
    shutil.rmtree(d, ignore_errors=True)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
