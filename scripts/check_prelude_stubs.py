# -*- coding: utf-8 -*-
"""闸① —— 预置头「假实现」数量上限锁 (只降不升)

背景（第 84 站 · 战线 1 补闸；源自 启元 复盘《补丁成屎山的机制与解法》 + 虾米 审计）：
  `compat_prelude.h` 里 有 大量 `#define 名(参) 常量` 形态的 **假实现** ——
  它们 **吞掉全部实参**，于是实参里的 **未声明符号 永远不会被解析**、报错永远不会出现；
  运行时 恒返回 常量。审计 的 原始 判据 是 计数（当时记 55，口径 未 固定）；
  本闸 把 口径 固定成 **名单级**（比 计数 更严 —— 改名 也 绕不过）。

规则（严格，可复现）：
  ① 只认 `#define 名(参)` —— 名 与 `(` 之间 **无空白**（C 标准：有空白 = 对象式宏，不是函数式宏）
  ② 先 去注释（块注释 + 行注释），再 去 字符串/字符 字面量
  ③ 参数 是否 出现 用 **词边界** 判定
  ④ 判据：**全部形参 都 未 出现** 在 宏体 里 ⟹ 全吞实参 = 假实现

用法：
  python scripts/check_prelude_stubs.py            # 检查（退出码 1 = 有新增）
  python scripts/check_prelude_stubs.py --write     # 用当前结果 重写 基线（只允许 在 减少 时用）
  python scripts/check_prelude_stubs.py --list      # 打印 全部 命中项
"""

# R715 仪器刀 (2026-10-11 虾米): 默认控制台 GBK 时 emoji(✅/❌) 会让 print 抛 UnicodeEncodeError
#   ⟹ 检查器 rc=1 假红 ⟹ 闸⑥/三套件 被中断。强推 UTF-8 输出（Python 3.7+，非 tty 管道下同样生效），
#   不改任何 print 文本；调用侧（verify.ps1 / run_tests_zh.py）另设 PYTHONIOENCODING=utf-8 作兜底。
import sys as _sys_utf8w
for _stream_utf8w in (getattr(_sys_utf8w, 'stdout', None), getattr(_sys_utf8w, 'stderr', None)):
    if _stream_utf8w is not None and hasattr(_stream_utf8w, 'reconfigure'):
        _stream_utf8w.reconfigure(encoding='utf-8', errors='replace')
import io, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRE = os.path.join(ROOT, 'compat_prelude.h')
BASE = os.path.join(ROOT, 'scripts', 'prelude_stub_baseline.txt')


def strip_lit(s):
    s = re.sub(r'"(\\.|[^"\\])*"', '""', s)
    s = re.sub(r"'(\\.|[^'\\])*'", "''", s)
    return s


# 第 101 站 (2026-09-18) 口径修正: 「本 就 该 是 空 宏」的 标准 允许形态 —— 它们 吞参 不是
# 债务 而是 **正确语义**, 计入 会 让 债务 数字 虚高、掩盖 真正 该 归真 的 项。
# 注意: 只 排除 **标准/规范 明确 允许为空** 的; 近似实现 (如 WIFEXITED->1)、
#       转义 stub (如 curl_*/pthread_*) 一律 **仍然 计入** —— 不得 用来 藏账。
LEGIT_EMPTY = {
    'assert':      'C 标准: NDEBUG 下 assert 展开为空 (6.3.2.2 允许); 吞参 是 定义 而非 缺陷',
    '__attribute__': 'GCC 扩展: 非 GCC 编译器 应 忽略 attribute (吞参 即 忽略), 语义正确',
    'va_end':      'C 标准: va_end 允许多种实现, 空操作 是 常见合法实现 (7.16.1.3)',
}


def scan(path):
    d = io.open(path, encoding='utf-8', errors='replace').read()
    d = re.sub(r'/\*.*?\*/', ' ', d, flags=re.S)      # 块注释
    d = re.sub(r'//[^\n]*', '', d)                     # 行注释
    hits = {}
    for line in d.split('\n'):
        # 宏名 允许 非 ASCII (甲言 支持 中文宏名); 名与 `(` 之间 无空白 = 函数式宏
        m = re.match(r'\s*#\s*define\s+([^\s(]+)\(([^)]*)\)\s*(.*)$', line)
        if not m:
            continue
        name, ps, body = m.group(1), m.group(2), m.group(3)
        if name in LEGIT_EMPTY:
            continue
        params = [q.strip() for q in ps.split(',') if q.strip()]
        if not params:
            continue
        b = strip_lit(body)
        if all(not re.search(r'\b%s\b' % re.escape(q), b) for q in params):
            hits[name] = (ps.strip(), body.strip())
    return hits


def load_base():
    if not os.path.exists(BASE):
        return None
    names = []
    for line in io.open(BASE, encoding='utf-8').read().split('\n'):
        line = line.strip()
        if line and not line.startswith('#'):
            names.append(line.split()[0])
    return set(names)


def main():
    args = sys.argv[1:]
    hits = scan(PRE)
    names = set(hits)
    if '--list' in args:
        for n in sorted(names):
            print('  %-34s (%s)  ->  %s' % (n, hits[n][0], hits[n][1][:64]))
        print('共 %d 个' % len(names))
        return 0
    if '--write' in args:
        lines = ['# 闸① 基线 —— 预置头 里「全吞实参 的 函数式宏」名单',
                 '# 规则: 见 scripts/check_prelude_stubs.py 头部。',
                 '# **只允许 减少**: 新增 名字 ⟹ 闸变红；修好一个就从这里删一行。',
                 '# 本文件由 `python scripts/check_prelude_stubs.py --write` 生成。',
                 '']
        for n in sorted(names):
            lines.append('%-34s (%s) -> %s' % (n, hits[n][0], hits[n][1][:48]))
        io.open(BASE, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
        print('基线 已写: %s (%d 个)' % (BASE, len(names)))
        return 0

    base = load_base()
    if base is None:
        print('[闸①] 基线缺失: %s —— 先跑 --write 建立基线' % BASE)
        return 1
    added = sorted(names - base)
    removed = sorted(base - names)
    print('[闸① 预置头假实现上限锁] 当前 %d 个 / 基线 %d 个' % (len(names), len(base)))
    if removed:
        print('  ✅ 已减少 %d 个 (记得用 --write 收紧基线): %s' % (len(removed), ' '.join(removed)))
    if added:
        print('  ❌ 新增 %d 个 —— 预置头不允许再添「吞实参的假实现」:' % len(added))
        for n in added:
            print('       %-34s (%s)  ->  %s' % (n, hits[n][0], hits[n][1][:50]))
        print('  （若确有必要：请在 台账 记明理由，并显式 --write 更新基线）')
        return 1
    print('  ✅ 无新增')
    return 0


if __name__ == '__main__':
    sys.exit(main())
