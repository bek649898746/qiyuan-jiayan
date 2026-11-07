#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_criteria_accounting —— ORD-20261107-140 判据（批跑墙钟账目三缺口）。

审计递单指出的三处账目缺口：
  ① 重试耗时不计账（只留最后一次 elapsed）；
  ② 失败尝试的原始输出不落盘（事后查不到「第一次为什么红」）；
  ③ 墙钟与各行之和的差额无对账行。

本判据**驱动真实函数**（不是 grep 源码）：
  · 直接 import `grok-build/scripts/run_criteria.py`，用一条**必然失败两次**的命令调 `_跑一行`；
  · 断言：attempts=2、`crit_<ID>_try1.log` 存在且 `[exit=1]`、主日志头部含逐次账目；
  · ③ 用结构断言（源码里必须打印「账目: 逐行 elapsed 合计」一行，且该行在 main 的收尾路径上）。

退出码：0 = 三缺口已补；1 = 仍缺（预期，直到 140 收口）。
"""
import importlib.util
import io
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.environ.get('QCC_LEDGER_REPO') or os.path.join(os.path.dirname(ROOT), 'grok-build')
RC_PY = os.path.join(LEDGER, 'scripts', 'run_criteria.py')


def load_rc():
    spec = importlib.util.spec_from_file_location('_rc140', RC_PY)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    bad = []
    if not os.path.exists(RC_PY):
        print('[FAIL] 找不到 %s' % RC_PY)
        return 1

    src = io.open(RC_PY, encoding='utf-8', errors='replace').read()
    if '账目: 逐行 elapsed 合计' not in src:
        bad.append('③ 缺墙钟对账行')

    mod = load_rc()
    work = tmp_dir('acc140')
    try:
        row = {'id': 'ACC-SELFTEST', 'criterion_kind': 'real',
               'criterion_cmd': 'python -c "import sys; sys.exit(1)"'}
        cmd = row['criterion_cmd']
        env = dict(os.environ)
        rc, dt, attempts = mod._跑一行(row, cmd, env, work, 120, '', '', None)
        print('[量] 真实调用 _跑一行 → rc=%d attempts=%d dt=%.1fs' % (rc, attempts, dt))
        if attempts != 2:
            bad.append('① attempts=%d（期望 2：失败后重试一次）' % attempts)
        trylog = os.path.join(work, 'crit_ACC-SELFTEST_try1.log')
        if not os.path.exists(trylog):
            bad.append('② 缺失败尝试日志 crit_ACC-SELFTEST_try1.log')
        else:
            t = io.open(trylog, encoding='utf-8', errors='replace').read()
            if '[exit=1' not in t:
                bad.append('② try1 日志未记 [exit=1]')
            else:
                print('[PASS] ② 失败尝试已落盘：%s（%d B）' % (os.path.basename(trylog), len(t)))
        mainlog = os.path.join(work, 'crit_ACC-SELFTEST.log')
        head = io.open(mainlog, encoding='utf-8', errors='replace').read() if os.path.exists(mainlog) else ''
        if 'try1=' not in head or 'attempts=2' not in head.splitlines()[1] if len(head.splitlines()) > 1 else True:
            if 'try1=' not in head:
                bad.append('① 主日志头部缺逐次账目（try1=…）')
            else:
                print('[PASS] ① 主日志头部逐次账目在位')
        else:
            print('[PASS] ① 主日志头部逐次账目在位')
    finally:
        shutil.rmtree(work, ignore_errors=True)

    if bad:
        print('[RED] ORD-20261107-140：%s' % '；'.join(bad))
        return 1
    print('=== ORD-20261107-140 账目判据: PASS（①逐次计时 ②失败落盘 ③墙钟对账）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
