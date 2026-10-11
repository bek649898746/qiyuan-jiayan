#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_typeword_clash.py —— 扫主源 参数名/局部声明名 是否 **撞甲言类型词**。

由来 (2026-09-20 第 165 站):
  我 在 `发浮读字段(整 节, 整 单)` 里 用 `单` 当 参数名 —— 而 `单` 是 甲言类型词 (单精度)。
  编译器 **不报错**, 但 生成的代码 行为错了 (feat_zh_struct_array_float_field_ok.c 值错乱:
  期望 1.0 2.0 3.0 4.0 实得 2.0 32.0 512.0 0.0)。
  ⚠️ 这 **不是第一次**: 第 144 站 也 因 变量名 `单`/`双` 撞类型词 而出事 (当时 改名 取回/指指)。
  ⟹ **我自己 踩了 同一个坑 两次** ⟹ 需要 机械检查 (人 会忘)。

检查方法:
  ① 从 主源 的 `逆低一()` (关键字表) 或 内置 类型词表 取 **类型词集合**
  ② 扫 所有 `静 <返回类型> <函数名>(<参数>) {` 定义, 提取 **参数名**
  ③ 扫 常见 声明模式 `<类型词> <名> =` / `<类型词> <名>;`, 提取 **声明名**
  ④ 报告 撞名

用法:  python scripts/check_typeword_clash.py [--all]
输出:  撞名 清单 (函数名 / 参数名 / 行号); --all 时 含 声明名。
"""

# R715 仪器刀 (2026-10-11 虾米): 默认控制台 GBK 时 emoji(✅/❌) 会让 print 抛 UnicodeEncodeError
#   ⟹ 检查器 rc=1 假红 ⟹ 闸⑥/三套件 被中断。强推 UTF-8 输出（Python 3.7+，非 tty 管道下同样生效），
#   不改任何 print 文本；调用侧（verify.ps1 / run_tests_zh.py）另设 PYTHONIOENCODING=utf-8 作兜底。
import sys as _sys_utf8w
for _stream_utf8w in (getattr(_sys_utf8w, 'stdout', None), getattr(_sys_utf8w, 'stderr', None)):
    if _stream_utf8w is not None and hasattr(_stream_utf8w, 'reconfigure'):
        _stream_utf8w.reconfigure(encoding='utf-8', errors='replace')
import io
import os
import re
import sys

JY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(JY, 'srclib_jiayan', 'qcc_zh.jy')

# 甲言类型词 / 保留语法词 (从 14_中文字符表 的 L0 语言本体 摘录 —— 只取「作名字会出问题」的那批)
TYPE_WORDS = {
    # 基本类型 (16/23)
    '字', '字节', '常', '双', '单', '整', '长', '长整', '短', '有符号', '无符号',
    '静', '无', '无整', '空', '易失', '数', '容', '双精', '单精',
    # 复合/派生 类型构造
    '枚', '构', '辖', '联合', '判别', '接口', '实现', '泛型', '类型', '存期',
    '线性', '复制', '析构', '密封', '签名', '函数', '指针', '数组', '引用', '借用',
    # 声明/语句 关键字 (作 参数名 大概率踩雷)
    '返', '若', '否', '循环', '断', '续', '例', '对照', '依据', '返传',
    # C 类型词 中文等价
    '整数', '浮点', '布尔',
    # ⚠️ 注意: **`字符` 不在此表** —— 第 147 站已厘清: `字符` **不是** 甲言类型名
    #    (char 的中文名 只有 `字` / `字节`; `字符` 是 合法 变量名, 主源自身 2933 行 用它 作 参数名)。
    #    首版工具 误把 `字符` 列入 ⟹ 报 3 处 假阳性 (汇发/接子/中发 的 参数名 就叫 字符, 是 历史 惯例)。
}

# ⚠️ 危害 **分层** (2026-09-20 第 165 站 实测):
#   · **词法层 类型词** (上表) —— 在 词法/解析 早期 就被 识别 为 类型词 ⟹ 作 参数名 **高危**,
#     编译器 甚至 不报错, 但 生成代码 行为错 (第 165 站: `单` ⟹ ASF 锚 值错乱)。
#   · **语句层 关键字** (`接口` / `构` / `枚` / `判别` ...) —— 只在 特定语句位置 识别 ⟹
#     作 参数名 **实测 可用** (主源 L5141 `接口方法返型(常 字 *接口, ...)` 与 L5156 `标方返型` 都
#     以 `接口` 作 参数名, 且 **被 真实调用** (L5158/L7929/L7945), 全量闸 746/746 通过)。
#     ⟹ 列 入 白名单, 但 **新代码 仍 建议避让** (语义会随 实现 演化)。
STMT_KEYWORDS_KNOWN_OK = {'接口'}

ALLOW = {'函', '名', '数', '值', '节', '型', '标', '序'} | STMT_KEYWORDS_KNOWN_OK   # 允许 的 短名单


def funcs(text):
    """产出 (行号, 返回类型, 函数名, 参数串)."""
    lines = text.split('\n')
    out = []
    pat = re.compile(r'^\s*静\s+([^(){},;]*?)\s*([\u4e00-\u9fffA-Za-z_][\w]*)\s*\(([^()]*)\)\s*\{')
    for i, l in enumerate(lines):
        if '(' not in l or '{' not in l or l.strip().startswith('/*') or l.strip().startswith('*'):
            continue
        m = pat.match(l)
        if not m:
            continue
        ret, name, params = m.group(1).strip(), m.group(2), m.group(3)
        out.append((i + 1, ret, name, params))
    return out


def param_names(params):
    """从 参数串 取 每段 的 最后一个 标识符 (参数名); 跳过 空/void."""
    out = []
    for seg in params.split(','):
        seg = seg.strip()
        if not seg or seg in ('空', 'void', ''):
            continue
        toks = re.findall(r'[\u4e00-\u9fff]+|[\w]+|\*', seg)
        toks = [t for t in toks if t != '*']
        if toks:
            out.append(toks[-1])
    return out


def main():
    text = io.open(SRC, encoding='utf-8', errors='ignore').read()
    hits = []
    for ln, ret, fname, params in funcs(text):
        for p in param_names(params):
            if p in TYPE_WORDS and p not in ALLOW:
                hits.append((ln, fname, p))
    print('=== 参数名 撞 类型词 ===')
    if not hits:
        print('  （无）')
    for ln, fname, p in hits:
        print('  L%-7d %s(...)  参数名「%s」撞类型词' % (ln, fname, p))
    print()
    print('检查 完成: %d 处 撞名' % len(hits))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
