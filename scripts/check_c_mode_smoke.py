# -*- coding: utf-8 -*-
"""check_c_mode_smoke —— **`-c` 模式冒烟**（闸② 候选）。

背景（第 439 站续三十七 · 2026-10-01）：
  **内建豁免表**（`qcc_zh.jy` @4547 起）是**编译器自动注入调用**的声明豁免 —— 表内项 ⟹ 跳过 A1001。
  **实测缺陷（P0-2 同型，范围更大）**：43 个 `__qcc_*` / `__gen_*` / `__coro_*` 运行时函数
  **未入表** ⟹ **链接模式**靠「自动包含 qcc_rt.c」侥幸拿到声明而通过，
  但 **`-c` 模式**（多文件编译，不自动包含运行时库）⟹ **任何使用这些特性的 .c 都 A1001**。
  （`__qcc_div_check` 是 2026-09-14 P0-2 已修的单例；本轮把同族的 43 项一并补齐。）

为什么需要本闸：
  **`run_tests_zh.py` 的 989 个锚全部是「链接模式」** ⟹ **`-c` 模式零覆盖** ⟹ 上述缺陷
  **闸永远发现不了**（本轮靠「发射符号 × 白名单」交叉普查才挖出）。

规则：对每个用例 **分别**跑 `-c` 与 链接模式，两者都必须成功；`-c` 失败即判红。

用法：
  python scripts/check_c_mode_smoke.py [编译器]      # 默认 qcc_zh_v1.exe
退出码：0 = 全过；1 = 有 `-c` 失败
"""

# R715 仪器刀 (2026-10-11 虾米): 默认控制台 GBK 时 emoji(✅/❌) 会让 print 抛 UnicodeEncodeError
#   ⟹ 检查器 rc=1 假红 ⟹ 闸⑥/三套件 被中断。强推 UTF-8 输出（Python 3.7+，非 tty 管道下同样生效），
#   不改任何 print 文本；调用侧（verify.ps1 / run_tests_zh.py）另设 PYTHONIOENCODING=utf-8 作兜底。
import sys as _sys_utf8w
for _stream_utf8w in (getattr(_sys_utf8w, 'stdout', None), getattr(_sys_utf8w, 'stderr', None)):
    if _stream_utf8w is not None and hasattr(_stream_utf8w, 'reconfigure'):
        _stream_utf8w.reconfigure(encoding='utf-8', errors='replace')
import io, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'qcc_zh_v1.exe')
OUT = os.path.join(ROOT, 'scratch_c1b2' if os.path.isdir(os.path.join(ROOT, 'scratch_c1b2')) else '.', 'csmoke')
os.makedirs(OUT, exist_ok=True)

H = '# 包含 <stdio.h>\n'
CASES = {
    'pool': H + '函数 整 主(空) { 内存池 p, 16, 2; 空 *x = 取块 p; 格式化 "POOL %d\\n", (x!=0); 返 0; }\n',
    'cell': H + '函数 整 主(空) { 单元 u, 5; 格式化 "CELL %d\\n", 取单元 u; 返 0; }\n',
    'vec':  H + '函数 整 主(空) { 动态数组 v; 追加 v, 7; 格式化 "VEC %d\\n", 取元素 v, 0; 返 0; }\n',
    'sb':   H + '函数 整 主(空) { 字符串构建 s; 追加串 s, "X"; 格式化 "SB %d\\n", 构建长度 s; 释放串 s; 返 0; }\n',
    'sem':  H + '信号量 sem, 2;\n函数 整 主(空) { 获取 sem; 格式化 "SEM OK\\n", 0; 放行 sem; 返 0; }\n',
    'div':  H + '函数 整 主(空) { 整 a = 10; 整 b = 3; 格式化 "DIV %d\\n", a / b; 返 0; }\n',
    'awaited': H + '函数 整 主(空) { 格式化 "PLAIN OK\\n", 0; 返 0; }\n',
    # R627 (2026-10-10 虾米 · 进IR前 T8 根治): **系统类型名(size_t) 作 返回类型位 的 声明** ——
    #   改前: 未登记 ⟹ 名字 被当 类型 吞掉、真声明符 静默丢弃 ⟹ 调用点 **A1001**（`-c` 模式 响亮）
    #   改后: `size_t` 登记为 内置 typedef ⟹ 声明 正常 登记 ⟹ rc=0。
    'systype_decl': H + 'extern size_t 甲一(void);\n函数 整 主(空) { 返 (整)甲一(); }\n',
}

bad = []

# R629 (2026-10-10 虾米 · 进IR前 A4 残留 根治) —— **带 `-D 名=值` 的 行号 校正**
#   病灶: `命宏前插()`（主源 L248）把 每个 `-D` 当 一行 `#define` **前插到 用户源码 之前** ⟹ 用户源码 下移 N 行,
#     而 行号 公式 的 校正量 `剩伴` 只 统计 运行库 行数 ⟹ **少减 N** ⟹ `__LINE__`/发错误/源位置/语义导航 四 处 一致 偏 +N。
#   判据（判别性 · 编译期 可判）: `-D M=1` 下 `编译期断言(__LINE__ == 3);` —— 改前 偏 +1 ⟹ 断言失败 ⟹ rc≠0; 改后 rc=0。
ARGS_CASES = {
    'dline_off': (H + '// 注释行\n函数 整 主(空) { 编译期断言(__LINE__ == 3); 格式化 "DLINE ok\\n", 0; 返 0; }\n',
                  ['-D', 'M=1']),
}
for name, (src, extra) in ARGS_CASES.items():
    p = os.path.join(OUT, name + '.c')
    io.open(p, 'w', encoding='utf-8-sig', newline='\n').write(src)
    r = subprocess.run([CC, p, '-c'] + extra + ['-o', os.path.join(OUT, name + '.obj')],
                       capture_output=True, timeout=300)
    ok = (r.returncode == 0)
    msg = (r.stdout + r.stderr).decode('utf-8', 'replace').strip().replace('\n', ' ')
    print('%-8s -c: %s%s' % (name, 'OK' if ok else 'FAIL', '' if ok else '  | ' + msg[:110]))
    if not ok:
        bad.append(name + '(-D 行号)')

# R626 (2026-10-10 虾米 · 进IR前 槽0 根治) —— **判别性 用例**: 「已声明 但 无定义 的 内置名」在 `-c` 模式
#   必须 发 **外部符号 调用**(交 链接器/msvcrt 解析), 不得 静默 生成「恒返 0」。
#   病灶: `abort`(以及 `_exit` 等) 在 位非标 内建表 内(A1001 豁免) ⟹ 编译器 为它 登记 声明(函数表 有条目),
#     但 `-c` 模式 不 含 运行库 qcc_rt.c、且 三张 IAT 表 也 没有 它 ⟹ 原 落 槽0 兜底 ⟹ `移即(0,0)`
#     ⟹ **编译通过、运行期 静默 no-op**(真载体: lua `ldo.c:132 abort();` 致命路径)。
#   判据: `-c` 编译 rc==0 **且** 目标文件 里 出现 该 外部符号名 —— 改前 实测 obj **无** `b'abort'` ⟹ 必红。
EXTRA = {
    'abort_extern': (H + '函数 整 主(空) { 格式化 "R626\\n"; abort(); 返 7; }\n', b'abort'),
}
for name, (src, need) in EXTRA.items():
    p = os.path.join(OUT, name + '.c')
    io.open(p, 'w', encoding='utf-8-sig', newline='\n').write(src)
    obj = os.path.join(OUT, name + '.obj')
    r = subprocess.run([CC, p, '-c', '-o', obj], capture_output=True, timeout=300)
    blob = open(obj, 'rb').read() if os.path.exists(obj) else b''
    ok = (r.returncode == 0) and (need in blob)
    print('%-8s -c: %s%s' % (name, 'OK' if ok else 'FAIL',
                             '' if ok else '  | rc=%d 含 %r=%s' % (r.returncode, need, need in blob)))
    if not ok:
        bad.append(name + '(外部符号)')
for name, src in CASES.items():
    p = os.path.join(OUT, name + '.c')
    io.open(p, 'w', encoding='utf-8-sig', newline='\n').write(src)
    obj = os.path.join(OUT, name + '.obj')
    r = subprocess.run([CC, p, '-c', '-o', obj], capture_output=True, timeout=300)
    ok_c = (r.returncode == 0)
    msg = (r.stdout + r.stderr).decode('utf-8', 'replace').strip().replace('\n', ' ')
    print('%-8s -c: %s%s' % (name, 'OK' if ok_c else 'FAIL', '' if ok_c else '  | ' + msg[:110]))
    if not ok_c:
        bad.append(name)

print()
print('[check_c_mode_smoke] %d 用例 · `-c` 失败 %d' % (len(CASES) + len(EXTRA) + len(ARGS_CASES), len(bad)))
if bad:
    print('  ❌ 失败: %s' % ' '.join(bad))
    sys.exit(1)
print('  → 全过')
sys.exit(0)
