# -*- coding: utf-8 -*-
"""并行验证驱动 — 并行跑 scripts/_verify_*.py (隔离目录, 修复临时名冲突).
用法: python scripts/run_verify_parallel.py [workers]
build.ps1 [5.5/5] 调用, 替代原串行 ForEach-Object."""
import subprocess, sys, os, glob, tempfile, shutil
from multiprocessing import Pool

sys.stdout.reconfigure(encoding='utf-8')
BASE = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
QCC = os.path.join(BASE, 'qcc_x86.exe')
WORKERS = int(sys.argv[1]) if len(sys.argv) > 1 else 8

def run_one(f):
    """在独立临时目录跑脚本 (scratch_test/ 隔离, qcc_x86.exe 复制进去)"""
    d = tempfile.mkdtemp(prefix='_vf_')
    try:
        os.makedirs(os.path.join(d, 'scratch_test'), exist_ok=True)
        shutil.copy(QCC, os.path.join(d, 'qcc_x86.exe'))
        r = subprocess.run(['python', f], cwd=d, capture_output=True, text=True,
                           encoding='utf-8', errors='replace')
        return os.path.basename(f), r.returncode
    except Exception as e:
        return os.path.basename(f), -1
    finally:
        shutil.rmtree(d, ignore_errors=True)

files = sorted(glob.glob(os.path.join(BASE, 'scripts', '_verify_*.py')))
print('并行验证 %d 个脚本 (workers=%d)...' % (len(files), WORKERS))
fails = []
with Pool(WORKERS) as pool:
    for name, rc in pool.imap_unordered(run_one, files):
        if rc != 0:
            fails.append(name)
            print('  [FAIL] %s rc=%d' % (name, rc))
print('== 验证脚本: PASS %d FAIL %d (%d 个) ==' % (len(files) - len(fails), len(fails), len(files)))
for f in fails:
    print('FAIL', f)
sys.exit(1 if fails else 0)
