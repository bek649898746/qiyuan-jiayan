# -*- coding: utf-8 -*-
"""check_systype627 —— R627「系统类型名 = 内置 typedef」复算器（T8 根治 的 判据面）。

背景（病灶）:
  甲言 **跳过 `<...>` 系统头** ⟹ `<stddef.h>`/`<stdint.h>`/`<sys/types.h>` 里的
  `size_t`/`int8_t`/`int64_t`/`off_t`/… **在本 TU 未登记** ⟹ **三面 同时 断**:
    ① 声明 `extern size_t f(void);` —— 名字 被当 类型 吞掉、真声明符 静默丢弃 ⟹ 调用点 A1001
       （真载体 lua `lobject.h` 的 `LUAI_FUNC size_t luaO_str2num(...)` ⟹ 6 个 lua 文件）
    ② 转型 `(size_t)x`  ③ `大小(size_t)`（lua `ldump.c` 实测 `A1001 大小: 'size_t' 未声明`）。
  修法: 解析() 复位后 把 该族 名 **登记为 内置 typedef**（按名 取宽度）+ `左系` 认作 类型名。

本器 判据（13 格 · 每格 只 报 rc）:
  d1  返回类型 = 未登记 系统类型名（无星）      —— 改前 A1001  ✗
  d2  返回类型 = 系统类型名 + 指针             —— 改前 A1001  ✗
  d3/d4 既有 通路 对照（已知 类型 + 星 / + const + 星）—— 改前 已 绿（零回归 对照）
  d7  二级指针 返回                            —— 改前 A1001  ✗
  d8  两条 同类 声明 + 两次 调用                —— 改前 A1001  ✗
  d5  用户 自己 typedef 同名（幂等 对照）        · d6 变量 面 · d9/d10 定义 面 · d11/d12 对照
  A 段 期望 **12/12 全绿**；B 段 = `probes\\r625_T8_返回类型位未知名\\*` 五探针，期望 **全绿**
  （y1 = 单 include `lobject.h` + 调用 `luaO_str2num` —— 改前 A1001）

用法: python scripts/check_systype627.py [编译器]      # 默认 qcc_zh_v1.exe
退出码: 0 = 全绿; 1 = 有 红
"""

# R715 仪器刀 (2026-10-11 虾米): 默认控制台 GBK 时 emoji(✅/❌) 会让 print 抛 UnicodeEncodeError
#   ⟹ 检查器 rc=1 假红 ⟹ 闸⑥/三套件 被中断。强推 UTF-8 输出（Python 3.7+，非 tty 管道下同样生效），
#   不改任何 print 文本；调用侧（verify.ps1 / run_tests_zh.py）另设 PYTHONIOENCODING=utf-8 作兜底。
import sys as _sys_utf8w
for _stream_utf8w in (getattr(_sys_utf8w, 'stdout', None), getattr(_sys_utf8w, 'stderr', None)):
    if _stream_utf8w is not None and hasattr(_stream_utf8w, 'reconfigure'):
        _stream_utf8w.reconfigure(encoding='utf-8', errors='replace')
import os, subprocess, sys

JY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(JY, 'qcc_zh_v1.exe')
TMP = os.path.join(os.environ.get('TEMP', '.'), '_systype627')
os.makedirs(TMP, exist_ok=True)
LUA = r'D:\qiyuan_home\corpus\lua-5.4.7'
env = dict(os.environ, QCC_DBG_SILENT='1')

CASES = {
    'd1_unk_nostar': 'extern size_t 甲一(void);\nint main(void){ return (int)甲一(); }\n',
    'd2_unk_star': 'extern size_t *甲二(void);\nint main(void){ return (int)甲二(); }\n',
    'd3_known_star': 'extern char *甲三(void);\nint main(void){ return (int)甲三(); }\n',
    'd4_known_const_star': 'extern const char *甲四(void);\nint main(void){ return (int)甲四(); }\n',
    'd5_typedef_unk': 'typedef unsigned long size_t;\nextern size_t 甲五(void);\nint main(void){ return (int)甲五(); }\n',
    'd6_unk_var': 'extern size_t 甲六;\nint main(void){ return (int)甲六; }\n',
    'd7_unk_typedef_star': 'extern size_t **甲七(void);\nint main(void){ return (int)甲七(); }\n',
    'd8_two_unk': 'extern size_t 甲八(void);\nextern size_t 乙八(void);\nint main(void){ return (int)甲八()+(int)乙八(); }\n',
    'd9_unk_def': 'size_t 甲九(int x) { return x + 1; }\nint main(void){ return (int)甲九(1); }\n',
    'd10_unk_def_star': 'size_t *甲十(int x) { return (size_t*)x; }\nint main(void){ return (int)(甲十(1) != 0); }\n',
    'd11_known_def': 'int 十一(int x) { return x + 1; }\nint main(void){ return 十一(1); }\n',
    'd12_unk_var_local': 'int main(void){ size_t 十二 = 3; return (int)十二; }\n',
}

bad = []
print('== A. 12 形态（编译器 %s）' % os.path.basename(CC))
for nm, src in CASES.items():
    p = os.path.join(TMP, nm + '.c')
    open(p, 'w', encoding='utf-8', newline='\n').write(src)
    r = subprocess.run([CC, '--lenient', '-c', p, '-o', os.path.join(TMP, nm + '.obj')],
                       capture_output=True, encoding='utf-8', errors='replace', env=env, timeout=120)
    t = (r.stdout or '') + (r.stderr or '')
    first = [l for l in t.splitlines() if l.startswith('[ERR]')]
    if r.returncode != 0:
        bad.append(nm)
    print('   %-22s rc=%d  %s' % (nm, r.returncode, (first[0][:90] if first else '')))

print('== B. T8 探针（probes/r625_T8_返回类型位未知名/*）')
probes = os.path.join(JY, 'probes', 'r625_T8_返回类型位未知名')
for f in sorted(os.listdir(probes)):
    if not f.endswith('.c'):
        continue
    r = subprocess.run([CC, '--lenient', '-D', '_WIN32=1', '-D', 'LUA_USE_WINDOWS', '-I', LUA,
                        '-c', os.path.join(probes, f), '-o', os.path.join(TMP, 't8.obj')],
                       capture_output=True, encoding='utf-8', errors='replace', env=env, timeout=180)
    if r.returncode != 0:
        bad.append(f)
    print('   %-30s rc=%d' % (f, r.returncode))

print()
print('[check_systype627] 红 %d' % len(bad))
if bad:
    print('  ❌ %s' % ' '.join(bad))
    sys.exit(1)
print('  → 全过')
sys.exit(0)
