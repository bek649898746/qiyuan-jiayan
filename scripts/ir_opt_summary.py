# -*- coding: utf-8 -*-
"""ir_opt_summary —— 汇总 IR / 优化器判据（16 个 check_ir_*.py + 5 个 check_opt_*.py）到一份日志。

用法:
  python -X utf8 scripts/ir_opt_summary.py <输出日志路径> [每条超时秒=900]

口径:
  · 每脚本独立进程，按脚本默认编译器（= qcc_zh_v2.exe，与 shipped v1 同一不动点）；
  · 末行写 `# 汇总: N 条，rc!=0 的 M 条`，M 必须为 0（`verify_plan_steps` 的 ir_opt 判据读这一行）。
由来（2026-10-26）：goal plan 的 Verification step 4 要求 `{SCRATCH}/ir_opt.log` 汇总 `bad=0`，
原先这份日志是手拼的；本脚本把「怎么汇总」固化成可复跑资产。
"""
import glob
import io
import os
import subprocess
import sys

if len(sys.argv) < 2:
    print(__doc__)
    sys.exit(2)
out_path = sys.argv[1]
timeout = int(sys.argv[2]) if len(sys.argv) > 2 else 900

names = sorted(glob.glob('scripts/check_ir_*.py')) + sorted(glob.glob('scripts/check_opt_*.py'))
print('脚本数', len(names))

buf = []
bad = 0
for p in names:
    cmd = [sys.executable, '-X', 'utf8', p]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                           errors='replace', timeout=timeout)
        rc = r.returncode
        out = (r.stdout or '') + (r.stderr or '')
    except subprocess.TimeoutExpired:
        rc = -999
        out = '[TIMEOUT] %ds' % timeout
    if rc != 0:
        bad += 1
    buf.append('$ python %s\n%s\n[exit=%d]\n' % (p.replace('\\', '/'), out.rstrip(), rc))
    print('%-40s rc=%d' % (os.path.basename(p), rc))

buf.append('\n# 汇总: %d 条，rc!=0 的 %d 条\n' % (len(names), bad))
io.open(out_path, 'w', encoding='utf-8', newline='\n').write('\n'.join(buf))
print('汇总写入', out_path, 'bad=%d' % bad)
sys.exit(0 if bad == 0 else 1)
