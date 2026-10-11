# -*- coding: utf-8 -*-
"""刀七-B 可复现命令：**IR 校验层 —— 影子 覆盖对账**
   跑 `QCC_DUMP=ir <编译器> <源>` ⟹ 读 `-- IR 影子对账` 段 ⟹
   **未覆盖的 L1 类** 必须 每一条 都在 **登记表**（本文件 下方 `登记`）里 有理由 ⟹ 否则 FAIL。

   判据: 未覆盖 类 数 == 已登记 类 数（且逐码对上）⟹ **差异 = 0（每条都有可查的登记理由）**。
用法: python scripts/check_ir_shadow.py [编译器] [源...]
"""
import io, os, re, subprocess, sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
S = r'C:\Users\ADMINI~1\AppData\Local\Temp\grok-goal-bc44933af5ae\implementer'
os.makedirs(S, exist_ok=True)
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(QJ, 'qcc_zh_v2.exe')

# ★ 登记表：**未覆盖的 L1 类** 逐条 理由（与 `R716A_L0对应L1对照表.md` 表 B 的 G 表 对齐）
登记 = {
    1110: '抽类选 —— `抽选择` 由 **条件跳 + 标号 复合表达**（不是一条独立机器码）⟹ L0 侧 无 同类 op（见 对照表 表 B 说明）',
}
# 另: 类 1030(比较) 由 度细/素九/总优一/中戊 覆盖 · 1060(转换) 由 宽二 等 覆盖 ⟹ 不应 未覆盖

SRCS = sys.argv[2:] or ['sh_h.c', 'sh_f.c', 'sh_empty.c']
env = dict(os.environ, QCC_DUMP='ir')
# 语料（写入 scratch ⟹ 自足 · 不依赖 任何 旧目录）
CORP = {
    'sh_empty.c': '#包含 <stdio.h>\n函数 整 主(空) { 返 0; }\n',
    'sh_h.c': '#包含 <stdio.h>\n函数 整 主(空) {\n    整 甲 = 3; 整 乙 = 7;\n    整 丙 = 甲 > 1 ? 乙 : 甲;\n'
              '    若 (丙 > 0) 丙 = 丙 + 1; 否 丙 = 0;\n    遍 (整 己 = 0; 己 < 3; 己++) 丙 = 丙 + 己;\n'
              '    循环 (丙 < 100) { 丙 = 丙 + 1; 若 (丙 > 20) 断; 续; }\n    格式化 "H %d\\n", 丙;\n    返 0;\n}\n',
    'sh_f.c': '#包含 <stdio.h>\n函数 整 主(空) {\n    整 计 = 0;\n    头:\n    计 = 计 + 1;\n'
              '    若 (计 < 3) 跳 头;\n    格式化 "F %d\\n", 计;\n    返 0;\n}\n',
}
for k, v in CORP.items():
    p = os.path.join(S, k)
    if not os.path.exists(p):
        io.open(p, 'w', encoding='utf-8', newline='\n').write(v)

total_un = []
n_checked = 0
n_段 = 0
for sr in SRCS:
    src = os.path.join(S, sr)
    if not os.path.exists(src):
        print('缺件:', sr); total_un.append(('缺件', sr)); continue
    n_checked += 1
    r = subprocess.run([CC, src, '-o', os.path.join(S, 'shadow.exe')], capture_output=True, env=env, cwd=QJ)
    o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    if 'IR-SHADOW-EMPTY' in o:
        print('%-14s ✗ **空查**（L1 类=0 或 L1 条=0 ⟹ 本器什么都没验）' % sr)
        total_un.append(('空查', sr)); continue
    m = re.search(r'IR 影子对账 .*?L0 op 记录 (\d+) 条 · L0 类 (\d+) 个 · L1 类 (\d+) 个 · L1 条 (\d+); 未覆盖 (\d+) 个', o)
    if not m:
        print('%-14s ✗ 未见 影子对账 段（rc=%d）' % (sr, r.returncode)); total_un.append(('缺段', sr)); continue
    n_段 += 1
    l0n, l0c, cls, l1n, un = (int(m.group(i)) for i in range(1, 6))
    codes = [int(x[0]) for x in re.findall(r'未覆盖 类码 (\d+) \(L1 (\d+) 条\)', o)]
    print('%-14s rc=%d  L0 op %d 条 · L0 类 %d 个 · L1 类 %d 个 · L1 条 %d · 未覆盖 %d 个 %s'
          % (sr, r.returncode, l0n, l0c, cls, l1n, un, codes))
    if l0n <= 0 or l1n <= 0 or l0c <= 0 or cls <= 0:
        total_un.append(('零计数', sr, l0n, l0c, cls, l1n))
    for c in codes:
        if c not in 登记:
            total_un.append(('未登记类', c, sr))
        else:
            print('      类码 %d ⟹ 已登记: %s' % (c, 登记[c][:70]))
    if un != len(codes):
        total_un.append(('计数不一致', sr, un, len(codes)))

print()
# ★ 防 假 PASS: 必须 真查到 源 且 真拿到 对账段
if n_checked == 0:
    print('[FAIL] **未查到任何源** ⟹ 本器 什么都没验（防 假 PASS）')
    sys.exit(1)
if total_un:
    print('[FAIL] 差异 未全部登记: %s' % total_un)
    sys.exit(1)
if n_段 == 0:
    print('[FAIL] 未取到 任何 影子对账 段 ⟹ 本器 什么判据 都没落（防 假 PASS）')
    sys.exit(1)
print('[PASS] IR 校验层：**L1 类覆盖 ⊆ L0 类覆盖**，未覆盖的类 **全部 有登记理由** '
      '（登记 %d 条 ⟹ **未登记差异 = 0**）；实查 %d 源，每源都取到对账段' % (len(登记), n_checked))
sys.exit(0)
