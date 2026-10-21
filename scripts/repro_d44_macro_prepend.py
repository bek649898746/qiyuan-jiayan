#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""D44 复现器 —— 「甲言 `宏` 定义 + exe 模式（前插运行库）」 ⟹ 词元流损坏。

【一句话判据】
  同一份带 `宏` 定义的用户源：
    · `-c` 模式（不拼运行库） ⟹ **永远 rc=0**
    · exe 模式（拼运行库） + 运行库足够长 ⟹ **rc=1**
      报 `[ERR] 缺右括号: N 个未闭合 (括号配对)` 或 `未找到 主 入口`
  用户源里 **一个 `宏` 定义都不写** 时，exe 模式 rc=0（所以与运行库单独无关）。

【2026-10-21 现场量化（本脚本可复算）】
  · 运行库 = N 条 `int gK = K;` 声明（无宏、无函数）
      129 条（1844 字节） ⟹ 通过
      130 条（≈1858 字节） ⟹ 失败
  · 固定 129 条 + 尾部补空格：pad=0 通过 / pad=100 失败 / **pad=1000 又通过**
    ⟹ 不是单调阈值，而是**按字节长度抖动的窗口**（一次实测：1859 通过、1860 失败、
    2844 又通过，周期≈350 字节）。⚠️ 窗口边界会随**用户源长度**平移（宏名加长 3 字节，
    边界就提前跨过），所以判据只取「通过/失败并存」这一事实，不钉死某个字节数。

【已排除】
  · 不是宏名撞运行库标识符（换成 `zzq_macro9` 一样红）
  · 不是运行库里的 `宏` 字样（把 `宏` 换成 `HONG` 一样红）
  · 不是 `容癸一` 没收集到宏（`-c` 模式同一份源展开正确）
  · 不是「遍 内嵌 若 + 断」控制流误编译（独立 30 行探针 和=163 正确）

【已定位】
  · 损坏发生在 `容癸一()` 的**重发**（`首布一(0, 词序)`）之后：
    输入流 664 词元 / 输出 656 词元（差值 8 = 宏定义的 8 个词元，符合预期），
    但输出流中间出现 `词终`(0) 洞 ⟹ `主置()` 的括号配对扫描提前中止。
  · 历史二分（用仓库里各代编译器二进制跑同一锚）：
      2026-10-14 17:00 `qcc_zh_th.exe`        ⟹ 6/6 绿
      2026-10-21 16:33 `qcc_zh_seed.exe`      ⟹ 6/6 红
    ⟹ 回归窗口 = **2026-10-15 .. 10-19**（C2 `#行界` / R797 表分 / round15 / D-OBJ-TRUNC 一带）。

【用法】
  python scripts/repro_d44_macro_prepend.py            # 默认 qcc_zh_v1.exe
  python scripts/repro_d44_macro_prepend.py --cc qcc_zh_seed.exe
  python scripts/repro_d44_macro_prepend.py --sweep     # 打印 0..2400 字节的 O/X 表

【退出码】
  0 = 复现成功（当前编译器仍有该缺陷）
  1 = 未复现（缺陷已修 ⟹ 应把本脚本转为正向判据：必须恒 rc=0）
"""
import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_d44')

USER_SRC = '宏 zzq_macro9(x) = x;\n函数 整 主(空) { 返 0; }\n'
DECLS = 129  # 现场实测：129 条基线（再 +1 条即跨过失败边界）


def runtime_text(n_decl, pad_spaces):
    body = ''.join('int g%d = %d;\n' % (i, i) for i in range(n_decl))
    return body + ' ' * pad_spaces


def run_cc(cc, mode, work):
    cmd = [os.path.abspath(os.path.join(ROOT, cc)), 'u.c', '-o', 'o' + ('.o' if mode else '.exe')]
    if mode:
        cmd.insert(1, '-c')
    env = dict(os.environ, QCC_SMALL_HEAP='1')
    p = subprocess.run(cmd, cwd=work, capture_output=True, env=env, timeout=300)
    msg = (p.stdout + p.stderr).decode('utf-8', 'replace').strip().replace('\n', ' | ')
    return p.returncode, msg[:120]


def prepare(work):
    if os.path.isdir(work):
        shutil.rmtree(work)
    os.makedirs(os.path.join(work, 'srclib'))
    with open(os.path.join(work, 'u.c'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(USER_SRC)


def compile_with(cc, n_decl, pad, work):
    with open(os.path.join(work, 'srclib', 'qcc_rt.c'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(runtime_text(n_decl, pad))
    return run_cc(cc, False, work)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--sweep', action='store_true')
    args = ap.parse_args()

    prepare(WORK)
    print('[D44] 编译器 = %s' % args.cc)

    # ---------- 对照 1: -c 模式（不拼运行库） ----------
    with open(os.path.join(WORK, 'srclib', 'qcc_rt.c'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(runtime_text(DECLS, 64))
    rc, msg = run_cc(args.cc, True, WORK)
    print('[D44] 对照①  -c  模式（不拼运行库）: rc=%d  %s' % (rc, msg))

    # ---------- 对照 2: exe 模式 + 极短运行库 ----------
    with open(os.path.join(WORK, 'srclib', 'qcc_rt.c'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('/* mini runtime */\n')
    rc, msg = run_cc(args.cc, False, WORK)
    print('[D44] 对照②  exe 模式 + 16 字节运行库: rc=%d  %s' % (rc, msg))

    # ---------- 主判据: 同一份源、只改运行库填充 ----------
    results = []
    for pad in (0, 15, 16, 100, 1000):
        rc, msg = compile_with(args.cc, DECLS, pad, WORK)
        nbytes = len(runtime_text(DECLS, pad).encode('utf-8'))
        results.append((pad, nbytes, rc))
        print('[D44] pad=%4d  运行库=%5d 字节  rc=%d  %s' % (pad, nbytes, rc, msg))

    if args.sweep:
        row = []
        for pad in range(0, 2401, 50):
            rc, _ = compile_with(args.cc, DECLS, pad, WORK)
            row.append('O' if rc == 0 else 'X')
        print('[D44] 0..2400 步长 50 的通过表（O=绿 X=红）:')
        print('      ' + ''.join(row))

    reproduced = any(rc != 0 for _, _, rc in results)
    flip = (results[0][2] == 0) != (results[2][2] == 0)
    print('[D44] 结论: %s（同一份源，仅改运行库尾部空格即翻转: %s）'
          % ('复现' if reproduced else '未复现', '是' if flip else '否'))
    return 0 if reproduced else 1


if __name__ == '__main__':
    sys.exit(main())
