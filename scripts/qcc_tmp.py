# -*- coding: utf-8 -*-
"""测试临时目录统一入口（ORD-20261102-90）。

为什么存在：135 个测试脚本里 111 个直接写 `%TEMP%`（C 盘），是 C 盘 Temp 涨到
25 GB 的直接原因；且「只落不清」。本模块把落点收敛到**一个**环境变量
`QCC_TMP_ROOT`（默认 `D:\\qcc_tmp`），并给清理脚本一个可识别的前缀根。

用法（测试脚本只允许走这两个函数，不再自己拼 TEMP）：
    from qcc_tmp import tmp_root, tmp_dir
    w = tmp_dir('qcc_mycheck')          # 新建带前缀子目录
    p = os.path.join(tmp_root(), 'x')   # 固定子路径（保留原语义）
"""
import os
import tempfile

DEFAULT_ROOT = r'D:\qcc_tmp'


def tmp_root():
    """测试产物统一根目录：`QCC_TMP_ROOT` 优先，默认 D 盘；不存在则创建。"""
    root = os.environ.get('QCC_TMP_ROOT') or DEFAULT_ROOT
    os.makedirs(root, exist_ok=True)
    return root


def tmp_dir(prefix):
    """在统一根下建一个带前缀的临时子目录，返回路径。

    前缀末尾的下划线会被去掉再统一补一个，避免出现 `qcc_x__ab12`。
    """
    p = (prefix or 'qcc').rstrip('_')
    return tempfile.mkdtemp(prefix=p + '_', dir=tmp_root())



# ═══ 子进程超时护栏（2026-11-04 · ORD-122）═══════════════════════════
# 为什么有它：实测 2026-11-04 20:34，`check_opt_per_pass.py` 跑 25 分钟只用 0.3s CPU，
#   卡在编译产物 `cm_02.asm.o.exe` 上 —— 产物死循环，而脚本那条路径没套 timeout ⟹ 永远等。
#   全仓 601 个调用子进程的判据脚本里，217 个（36%）没有超时保护。
#
# 做法：在共享模块里给 `subprocess.run` 打一个**默认超时**补丁。
#   脚本自己传了 timeout 就尊重它；没传的，按用途给默认值：
#     · 构建/编译类（build_zh / gcc / ld / make …）→ 600s
#     · 其余（跑被测产物等）→ 60s
#   可用环境变量覆盖：QCC_SUBPROC_TIMEOUT / QCC_SUBPROC_BUILD_TIMEOUT
#
# 判据：超时抛 `subprocess.TimeoutExpired` ⟹ 脚本红，**不静默、不永久挂**。
import subprocess as _subprocess

_DEFAULT_TIMEOUT = float(os.environ.get('QCC_SUBPROC_TIMEOUT') or 60.0)
_BUILD_TIMEOUT = float(os.environ.get('QCC_SUBPROC_BUILD_TIMEOUT') or 600.0)
_BUILD_HINTS = ('build_zh', 'gcc', 'g++', 'cc1', 'ld.exe', 'make', 'cmake',
                'qcc_x86', 'asm_zh', 'msbuild', 'cl.exe')

_orig_run = _subprocess.run


def _正常化(cmd):
    if isinstance(cmd, (list, tuple)):
        return ' '.join(str(x) for x in cmd)
    return str(cmd)


def _run_with_timeout(*args, **kwargs):
    """给 subprocess.run 补默认超时（脚本显式传 timeout 时不覆盖）。"""
    if kwargs.get('timeout') is None:
        cmd = args[0] if args else kwargs.get('args', '')
        low = _正常化(cmd).lower()
        kwargs['timeout'] = _BUILD_TIMEOUT if any(h in low for h in _BUILD_HINTS) else _DEFAULT_TIMEOUT
    return _orig_run(*args, **kwargs)


_run_with_timeout.__doc__ = ('带默认超时的 subprocess.run：构建类 %.0fs / 其余 %.0fs；'
                             '显式 timeout 优先；超时抛 TimeoutExpired' % (_BUILD_TIMEOUT, _DEFAULT_TIMEOUT))
_subprocess.run = _run_with_timeout
# ══════════════════════════════════════════════════════════════════
