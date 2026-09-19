# -*- coding: utf-8 -*-
"""守卫: 运行库 (qcc_rt.c) 查找链 —— 第 138 站 C138。

由来: 编译器 会 在 「整数 除/取余 + 除数 非常量」 时 把 除数 替换为 `__qcc_div_check64(除数)` 调用
      (AST 层, 主源 乘法() L8817); 该 函数 的 **实现** 在 运行库 `qcc_rt.c` 里, 由 编译器 在
      **链接模式** 自动 prepend。若 运行库 查找 失败 且 **静默** (原行为: 直接 返 NULL),
      用户 只 见「变量除法 崩 (0xC0000094 除零)」, 不 知道 是 运行库 缺失。

本守卫 验证 三件 (均 在 **仓库外** 目录 执行, 复现 cwd 依赖):
  ① 运行库 找不到 时 **必须 响亮警告** (不 得 静默);
  ② 设置 `QCC_RT_PATH` 后, 跨目录 编译 **正常** (产物 含 运行库 且 变量除法 结果 正确);
  ③ 产物 大小 差异 可 作为 「运行库 是否 prepend」的 指纹 (7592 vs 149507 字节量级)。

用法: python scripts/check_rt_lookup.py [qcc.exe]
退出码: 0 = 全通过; 1 = 有 不符
"""
import io, os, re, subprocess, sys, tempfile

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

# ---- ① 仓库外 + 无 QCC_RT_PATH ⟹ 必须 有 警告 (且 产物 小 / 运行 崩) ----
env = dict(os.environ)
env.pop('QCC_RT_PATH', None)
p = subprocess.run([QCC, src, '-o', exe], capture_output=True, cwd=tmp, env=env)
msg = ((p.stdout or b'') + (p.stderr or b'')).decode('utf-8', 'replace')
size_no_rt = os.path.getsize(exe) if os.path.exists(exe) else 0
has_warn = ('警告' in msg and 'qcc_rt.c' in msg)
print('[①] 仓库外 无 QCC_RT_PATH: 产物 %d 字节; 警告=%s' % (size_no_rt, '有' if has_warn else '无'))
if not has_warn:
    fails.append('① 运行库 缺失 时 未 响亮 警告 (静默 失败 会 把 运行库缺失 伪装成 除法 崩)')

# ---- ② 仓库外 + QCC_RT_PATH ⟹ 必须 正常 ----
env2 = dict(env, QCC_RT_PATH=RT)
p2 = subprocess.run([QCC, src, '-o', exe], capture_output=True, cwd=tmp, env=env2)
size_rt = os.path.getsize(exe) if os.path.exists(exe) else 0
out = ''
if p2.returncode == 0 and os.path.exists(exe):
    r = subprocess.run([exe], capture_output=True)
    out = (r.stdout or b'').decode('utf-8', 'replace').strip()
    rc = r.returncode
else:
    rc = -1
print('[②] 仓库外 + QCC_RT_PATH: 产物 %d 字节; 输出=%s rc=%d' % (size_rt, out or '(空)', rc))
if out != '142857142857' or rc != 0:
    fails.append('② 设置 QCC_RT_PATH 后 仍 未 正常 (输出 %r rc=%d)' % (out, rc))

# ---- ③ 指纹: 含运行库 的 产物 应 显著 大 ----
print('[③] 大小 指纹: 无运行库 %d vs 有运行库 %d' % (size_no_rt, size_rt))
if not (size_rt > size_no_rt * 2):
    fails.append('③ 运行库 prepend 的 大小 指纹 不 成立 (%d vs %d)' % (size_no_rt, size_rt))

if fails:
    print('\n[失败 FAIL] 运行库 查找链 守卫:')
    for f in fails:
        print('  -', f)
    sys.exit(1)
print('\n[通过 PASS] 运行库 查找链 守卫 (警告 + 环境变量 修复 + 大小 指纹)')
