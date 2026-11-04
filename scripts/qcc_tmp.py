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
