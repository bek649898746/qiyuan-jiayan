# -*- coding: utf-8 -*-
"""守卫: 运行库 (qcc_rt.c) 查找链 —— 第 138 站 C138 立, 第 139 站 C139 更新期望。

背景: 编译器 在 「整数 除/取余 + 除数 非常量」 时 把 除数 替换为 `__qcc_div_check64(除数)` 调用
      (AST 层, 主源 乘法() L8817); 该 实现 在 运行库 `qcc_rt.c`, 由 编译器 在 链接模式 自动 prepend。
      若 运行库 查找 失败 且 静默 (原行为), 用户 只 见「变量除法 崩 (0xC0000094)」, 不 知 真因。

第 138 站: 加 环境变量 `QCC_RT_PATH` + 找不到时 响亮 警告 (治 静默)。
第 139 站: 修 内核库「名→槽」查询 (用户代码 调用 GetCommandLineA 原本恒返 0) ⟹ 第 138 站 的
          「命令行 取 exe 路径」级 复活 ⟹ **仓库外 编译 自动 找到 运行库** (不再需要 环境变量)。

本守卫 验证 三件 (均 在 **仓库外** 目录 执行):
  ① 仓库外 + 无 `QCC_RT_PATH` ⟹ **自动 找到 运行库** (产物 含 运行库, 变量除法 结果 正确, **无 警告**);
  ② 仓库外 + 设 `QCC_RT_PATH` ⟹ 显式 覆盖 仍 有效 (同样 正确);
  ③ 产物 大小 差异 可 作 「运行库 是否 prepend」的 指纹。

用法: python scripts/check_rt_lookup.py [qcc.exe]
退出码: 0 = 全通过; 1 = 有 不符
"""
import io, os, re, subprocess, sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QCC = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(ROOT, 'qcc_zh_seed.exe')
RT = os.path.join(ROOT, 'srclib', 'qcc_rt.c')

PROBE = r'''#include <stdio.h>

int main(void) {
    long long x = 1000000000000LL, y = 7;
    printf("%lld\n", x / y);
    return 0;
}
'''

tmp = os.path.join(os.environ.get('TEMP', '.'), '_rt_lookup_check')
os.makedirs(tmp, exist_ok=True)
src = os.path.join(tmp, 'probe_div.c')
io.open(src, 'w', encoding='utf-8').write(PROBE)
exe = os.path.join(tmp, 'probe.exe')

fails = []


def build_env(with_path):
    e = dict(os.environ)
    if with_path:
        e['QCC_RT_PATH'] = RT
    else:
        e.pop('QCC_RT_PATH', None)
    return e


def build_and_run(env):
    p = subprocess.run([QCC, src, '-o', exe], capture_output=True, cwd=tmp, env=env)
    msg = ((p.stdout or b'') + (p.stderr or b'')).decode('utf-8', 'replace')
    size = os.path.getsize(exe) if os.path.exists(exe) else 0
    out, rc = '', -1
    if p.returncode == 0 and os.path.exists(exe):
        r = subprocess.run([exe], capture_output=True)
        out = (r.stdout or b'').decode('utf-8', 'replace').strip()
        rc = r.returncode
    return msg, size, out, rc


# ---- ① 仓库外 + 无 QCC_RT_PATH ⟹ 应 自动 找到 (无警告 + 正确) ----
msg, size_no, out_no, rc_no = build_and_run(build_env(False))
has_warn = ('警告' in msg and 'qcc_rt.c' in msg)
print('[①] 仓库外 无 QCC_RT_PATH: 产物 %d 字节; 输出=%s rc=%d; 警告=%s'
      % (size_no, out_no or '(空)', rc_no, '有' if has_warn else '无'))
if has_warn:
    fails.append('① 仓库外 编译 仍 报 「运行库 未找到」 —— 路径 探测 级 未 生效')
if out_no != '142857142857' or rc_no != 0:
    fails.append('① 仓库外 编译 未 自动 找到 运行库 (输出 %r rc=%d)' % (out_no, rc_no))

# ---- ② 仓库外 + QCC_RT_PATH ⟹ 显式 覆盖 仍 有效 ----
msg2, size_rt, out_rt, rc_rt = build_and_run(build_env(True))
print('[②] 仓库外 + QCC_RT_PATH: 产物 %d 字节; 输出=%s rc=%d'
      % (size_rt, out_rt or '(空)', rc_rt))
if out_rt != '142857142857' or rc_rt != 0:
    fails.append('② 设置 QCC_RT_PATH 后 未 正常 (输出 %r rc=%d)' % (out_rt, rc_rt))

# ---- ③ 指纹: 含运行库 的 产物 应 显著 大 (两者 都 应 含 运行库) ----
print('[③] 大小 指纹: 自动查找 %d vs 环境变量 %d' % (size_no, size_rt))
if not (size_no > 20000 and size_rt > 20000):
    fails.append('③ 产物 未 含 运行库 (自动 %d / 环境变量 %d 字节)' % (size_no, size_rt))

if fails:
    print('\n[失败 FAIL] 运行库 查找链 守卫:')
    for f in fails:
        print('  -', f)
    sys.exit(1)
print('\n[通过 PASS] 运行库 查找链 守卫 (自动查找 + 环境变量覆盖 + 大小指纹)')
