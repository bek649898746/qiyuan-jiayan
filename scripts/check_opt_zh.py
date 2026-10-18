# -*- coding: utf-8 -*-
"""check_opt_zh.py —— 甲言优化器（opt_zh.jy）判据器。

对应 plan 验收 1 / 2：
  ① **甲言源能被本仓终态编译器编译** ⟹ rc=0 且产物非空（真驱动编译，不是看定义存在）
  ② **真干活**：运行该产物处理仓内 IR ⟹ 输出**非空、结构可解析**（不是空壳、不是原样回显）
  ③ **真做变换**：输出与输入**确有可复算的差异**（逐条差异非空）
  ④ **不改变程序行为**：对输入与输出**各自求值**（本判据器内置独立求值器，非重实现优化器）
     ⟹ **两者语义值相同**

用法: python scripts/check_opt_zh.py [编译器]
退出码: 0 = 四条全过
"""
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(QJ)
TMP = os.path.join(os.environ.get('TEMP', '.'), '_opt_zh')
os.makedirs(TMP, exist_ok=True)


def find_qcc():
    env = os.environ.get('QCC_TEST_BIN')
    if env and os.path.exists(env):
        return env
    for n in range(7, 0, -1):
        p = os.path.join(QJ, 'qcc_zh_v%d.exe' % n)
        if os.path.exists(p):
            return p
    return os.path.join(QJ, 'qcc_zh_seed.exe')


CC = sys.argv[1] if len(sys.argv) > 1 else find_qcc()
OPT_SRC = os.path.join(QJ, 'srclib_jiayan', 'opt_zh.jy')
OPT_EXE = os.path.join(TMP, 'opt_zh.exe')
IR_IN = os.path.join(QJ, 'scripts', 'probe', 'opt_zh_in.ir')

fails = []
print('[check_opt_zh] 编译器 = %s' % CC)

# ① 编译甲言优化器
r = subprocess.run([CC, OPT_SRC, '-o', OPT_EXE], capture_output=True, cwd=QJ)
if r.returncode != 0 or not os.path.exists(OPT_EXE) or os.path.getsize(OPT_EXE) == 0:
    print('  ① ✗ 编译失败 rc=%d' % r.returncode)
    print(((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')[:400])
    sys.exit(1)
print('  ① 甲言优化器 编译 rc=0 · 产物 %d 字节  ✓' % os.path.getsize(OPT_EXE))

# ② 运行处理 IR（输出写文件 —— 与 header 的 `-o out.ir` 用法一致）
main_out = os.path.join(TMP, 'main_out.ir')
if os.path.exists(main_out):
    os.remove(main_out)
r2 = subprocess.run([OPT_EXE, IR_IN, '-o', main_out], capture_output=True, cwd=QJ)
out = open(main_out, encoding='utf-8', errors='replace').read() if os.path.exists(main_out) else ''
if not out:
    out = (r2.stdout or b'').decode('utf-8', 'replace')   # 兜底: 若无 -o 行为
if r2.returncode != 0:
    print('  ② ✗ 运行 rc=%d' % r2.returncode)
    print((r2.stderr or b'').decode('utf-8', 'replace')[:400])
    sys.exit(1)
body = [l for l in out.split('\n') if l.strip() and not l.startswith('#')]
if not body:
    print('  ② ✗ 输出为空'); sys.exit(1)
print('  ② 运行 rc=0 · 非空输出 %d 条指令  ✓' % len(body))

src_txt = open(IR_IN, encoding='utf-8').read()
src_body = [l for l in src_txt.split('\n') if l.strip() and not l.startswith('#')]

# ③ 差异非空
diff = [l for l in src_body if l not in body]
print('  ③ 与输入逐条差异 %d 条（输入 %d → 输出 %d）  %s'
      % (len(diff), len(src_body), len(body), '✓' if diff else '✗ 原样回显'))
if not diff:
    fails.append('输出与输入无差异 ⟹ 未真做变换')

# ④ 语义等价：独立求值器（只算值，不做变换）
RE_DEF = re.compile(r'^(常量|加|减|乘)\s+(\S+)\s+(\S+)(?:\s+(\S+))?$')
RE_RET = re.compile(r'^返回\s+(\S+)$')


def 求值(lines):
    env = {}
    ret = None
    for l in lines:
        l = l.strip()
        m = RE_RET.match(l)
        if m:
            v = m.group(1)
            ret = env[v] if v in env else int(v)
            continue
        m = RE_DEF.match(l)
        if not m:
            continue
        op, dst, a, b = m.group(1), m.group(2), m.group(3), m.group(4)
        ga = env[a] if a in env else int(a)
        if op == '常量':
            env[dst] = ga
            continue
        gb = env[b] if b in env else int(b)
        env[dst] = ga + gb if op == '加' else (ga - gb if op == '减' else ga * gb)
    return ret


vs, vo = 求值(src_body), 求值(body)
ok4 = (vs is not None and vs == vo)
print('  ④ 语义: 输入求值=%s · 输出求值=%s ⟹ %s' % (vs, vo, '✓ 相同' if ok4 else '✗ 不同'))
if not ok4:
    fails.append('优化前后语义值不同 (%s vs %s)' % (vs, vo))

# ⑤ `-o` 真写文件（不得是死 flag）
outfile = os.path.join(TMP, 'out_check.ir')
if os.path.exists(outfile):
    os.remove(outfile)
r5 = subprocess.run([OPT_EXE, IR_IN, '-o', outfile], capture_output=True, cwd=QJ)
ok5 = (r5.returncode == 0 and os.path.exists(outfile) and os.path.getsize(outfile) > 0)
print('  ⑤ -o 真写文件: rc=%d 文件=%s ⟹ %s'
      % (r5.returncode, os.path.exists(outfile), '✓' if ok5 else '✗ 死 flag'))
if not ok5:
    fails.append('-o 解析了却未写文件（与 header/用法串 不符）')

# ⑥ 重定义 / 遮蔽语义（最近定义生效）
RD = os.path.join(QJ, 'scripts', 'probe', 'opt_zh_redef.ir')
rd_out = os.path.join(TMP, 'redef_out.ir')
if os.path.exists(rd_out):
    os.remove(rd_out)
r6 = subprocess.run([OPT_EXE, RD, '-o', rd_out], capture_output=True, cwd=QJ)
rd_txt = open(rd_out, encoding='utf-8', errors='replace').read() if os.path.exists(rd_out) else ''
rd_body = [l for l in rd_txt.split('\n') if l.strip() and not l.startswith('#')]
rd_src = [l for l in open(RD, encoding='utf-8').read().split('\n') if l.strip() and not l.startswith('#')]
v_in, v_out = 求值(rd_src), 求值(rd_body)
ok6 = (r6.returncode == 0 and v_in is not None and v_in == v_out == 4)
print('  ⑥ 重定义/遮蔽: 输入求值=%s · 输出求值=%s ⟹ %s' % (v_in, v_out, '✓ 最近定义生效' if ok6 else '✗'))
if not ok6:
    fails.append('重定义输入被误折叠（最近定义未生效）: %s vs %s' % (v_in, v_out))

# ⑦ 无 -o ⟹ 响亮拒绝，且不得产生垃圾文件（曾出现: 默认 outf="-" 被 fopen ⟹ 生成名为 - 的文件）
import shutil
dash_dir = os.path.join(TMP, 'noo')
if os.path.isdir(dash_dir):
    shutil.rmtree(dash_dir)
os.makedirs(dash_dir, exist_ok=True)
r7 = subprocess.run([OPT_EXE, os.path.abspath(IR_IN)], capture_output=True, cwd=dash_dir)
junk = os.path.join(dash_dir, '-')
ok7 = (r7.returncode != 0 and not os.path.exists(junk))
print('  ⑦ 无 -o: rc=%d · 垃圾文件 - 存在=%s ⟹ %s'
      % (r7.returncode, os.path.exists(junk), '✓ 响亮拒绝且不产垃圾' if ok7 else '✗'))
if not ok7:
    fails.append('无 -o 路径未响亮拒绝，或产生了名为 - 的垃圾文件')

print()
if fails:
    print('[FAIL] opt_zh 判据 不成立:')
    for f in fails:
        print('   -', f)
    sys.exit(1)
print('[PASS] opt_zh: 甲言源可编译 · 真跑出非空可解析 IR · 变换差异非空 · 语义值不变 · -o 真写文件 · 重定义最近定义生效 · 无 -o 响亮拒绝不产垃圾')
sys.exit(0)
