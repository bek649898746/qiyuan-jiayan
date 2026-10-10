# -*- coding: utf-8 -*-
"""check_r630_lvm —— R630「指针算术 作 链基」复算器（闸⑦）。

病灶（`内存地址()` 缺「rvalue 链基」分支）:
  `内存地址()` 的分支族只处理 **左值** 基 —— 变量 / 解引用 / 箭头 / 下标 / 成员 / 取址（R617 补）。
  `(p ± k)` 这类 **rvalue 链基** 落 **L5885 总兜底** `静默计(3, "内存地址.链未解析(兜底-1)")` + 返 -1
  ⟹ 链基 **地址被丢弃** ⟹ 上层回退标量读 ⟹ 静默错值 / 假结构。
  ⚠️ 其上方那个自称「加(+) / 减(-)」的分支，判据写的是 `节点_作用域块 || 节点_返节`
     —— 二者是 **语句** 节点（见 `码生成` 语句 switch 与 `节点判` 映射: 2=二元 / 5=作用域块 / 6=返节），
     表达式里不会出现 ⟹ 那个分支是 **死代码**（注释与判据不符，是历史笔误）。

修法（单点 · 输入源头级别）:
  补 `若 (节型别[数] == 节点_二元 && 节点指元素(数) > 0)` 分支 —— 指针算术是 rvalue，
  其 **值** 即链基的地址 ⟹ `码生成(数)` 直接求值（结果落堆九，与既有 `__tern_` 物化分支同契约），
  再 **局部** 穿透左操作数取目标构序（不改全局 `序七`，避免波及数十个既有调用点）。
  判据用 `节点指元素(数) > 0`（其自身分支已限定二元 +/-）⟹ 语义即「这是指针算术」，
  **不依赖运算符编码常量**。

判据（同文件自对照 · 实测值）:
  lua `lvm.c` 兜底计数:   **344  →  0**      ← 本刀靶区（全语料最大单点）
  obj 字节:               4308506 → 4319133   （+10.6 KB ⟹ 原先被丢弃的代码现在真发射）
  该文件载体: 算术/比较宏族（`op_arithI`/`op_arithf`/… 含 `s2v(ra + k)` 与 `vRB(i)` 展开）
          + `forprep`/`floatforloop` 的 `s2v(ra + 1..3)`。

⚠️ 诚实边界（同批如实记录）:
  · 本刀只清 **lvm.c** 的 344 次；lua 语料整体 兜底仍有 **3 次残留**（在别的文件），
    与 zlib 的 **3 次** 同属 **下一刀** 靶区（形态待定，见 `probes\\r630_指针算术链基\\README.md`）。
  · 探针族 p3/p4/p5/p7（表达式内取值/取址、初始化 RHS、宏初始化、真 lua 头文件复刻）**均未复现**该兜底
    —— 故本器以 **真实语料** 为判据（这是「量具先于结论」的又一次应用）。

用法: python scripts/check_r630_lvm.py [编译器]      # 默认 qcc_zh_v1.exe
退出码: 0 = 全绿; 1 = 有红
"""
import os, subprocess, sys

JY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(JY, 'qcc_zh_v1.exe')
TMP = os.path.join(os.environ.get('TEMP', '.'), '_r630_lvm')
os.makedirs(TMP, exist_ok=True)
LUA = r'D:\qiyuan_home\corpus\lua-5.4.7'
BASELINE_BEFORE = 344          # R629 实测（前值）
env = dict(os.environ, QCC_DBG_SILENT='1')


def main():
    src = os.path.join(LUA, 'lvm.c')
    if not os.path.exists(src):
        print('  [skip] 语料 %s 不存在 — 跳过（不判红）' % src)
        return 0
    out = os.path.join(TMP, 'lvm.obj')
    if os.path.exists(out):
        os.remove(out)
    r = subprocess.run([CC, '--lenient', '-D', '_WIN32=1', '-D', 'LUA_USE_WINDOWS',
                        '-I', LUA, '-c', src, '-o', out],
                       capture_output=True, encoding='utf-8', errors='replace', env=env, timeout=900)
    t = (r.stdout or '') + (r.stderr or '')
    n3 = t.count('链未解析')
    sz = os.path.getsize(out) if os.path.exists(out) else 0
    print('  编译器 = %s' % os.path.basename(CC))
    print('  lvm.c 编译 rc=%d ; 链未解析(槽3) 计数 = %d ; obj 字节 = %d' % (r.returncode, n3, sz))
    if r.returncode != 0:
        bad = [l for l in t.splitlines() if '[ERR]' in l]
        print('  ❌ 编译失败: %s' % (bad[0][:120] if bad else ''))
        return 1
    if n3 != 0:
        print('  ❌ 槽3 残留 %d 次（本刀前值 %d ⟹ 修复未生效或未落地）' % (n3, BASELINE_BEFORE))
        return 1
    print('  ✅ 槽3 == 0（本刀前值 %d）' % BASELINE_BEFORE)
    return 0


if __name__ == '__main__':
    sys.exit(main())
