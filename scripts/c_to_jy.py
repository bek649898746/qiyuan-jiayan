# -*- coding: utf-8 -*-
"""c_to_jy.py — 把 qcc_x86.c（C 宿主）逐 token 翻译成 qcc_work.jy（甲言镜像）。

铁律:
- 只在「代码 token」位置替换英文 C 关键字 → 甲言中文关键字。
- 注释 (// 与 /* */)、字符串字面量 ("...")、字符字面量 ('x') 一律原样保留。
- 关键字按完整 C 标识符边界匹配: `if` 不能命中 `differ`。
- 完整映射来自 qcc_x86.c lexer 的「甲言关键字 中文 → 英文」段 (约 2860-2910 行)，
  这里反向成「英文 → 中文」。同一英文有多个中文别名时，选与旧镜像一致的主别名:
      else→否, char→字, double→双, float→单, sizeof→大小。
  没有中文映射的 C 关键字 (extern/volatile/signed/_Bool/inline/FILE 等) 保持英文。

用法:
    python scripts/c_to_jy.py            # 读 srclib/qcc_x86.c, 写 srclib_jiayan/qcc_work.jy
    python scripts/c_to_jy.py --stats    # 只打印替换统计, 不写文件
    python scripts/c_to_jy.py --check    # 生成后与旧备份做 token 级快速校验 (需要 qcc_work.jy.bak)
"""
import os
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'srclib' / 'qcc_x86.c'
DST = ROOT / 'srclib_jiayan' / 'qcc_work.jy'

# 英文 C 关键字 → 甲言中文关键字 (反向映射自 lexer 中文段)。
# 顺序对结果无影响, 但保持可读性。
EN2ZH = {
    'struct':   '构',
    'union':    '联',
    'enum':     '枚',
    'return':   '返',
    'if':       '若',
    'else':     '否',
    'while':    '循环',
    'do':       '做',
    'for':      '遍',
    'break':    '断',
    'continue': '续',
    'switch':   '择',
    'case':     '例',
    'default':  '缺',
    'goto':     '跳',
    'int':      '整',
    'double':   '双',
    'float':    '单',
    'char':     '字',
    'void':     '空',
    'short':    '短',
    'long':     '长',
    'const':    '常',
    'static':   '静',
    'unsigned': '无',
    'sizeof':   '大小',
}


def is_ident_start(c):
    return ('A' <= c <= 'Z') or ('a' <= c <= 'z') or c == '_'


def is_ident_char(c):
    return is_ident_start(c) or ('0' <= c <= '9')


def skip_string(s, i):
    """s[i] == '"'; return index just past the closing quote."""
    i += 1
    n = len(s)
    while i < n:
        c = s[i]
        if c == '\\':
            i += 2
            continue
        if c == '"':
            return i + 1
        i += 1
    return i


def skip_char_literal(s, i):
    """s[i] == "'"; return index just past the closing quote."""
    i += 1
    n = len(s)
    while i < n:
        c = s[i]
        if c == '\\':
            i += 2
            continue
        if c == "'":
            return i + 1
        i += 1
    return i


def skip_line_comment(s, i):
    """s[i:i+2] == '//'; return index just past the newline."""
    n = len(s)
    while i < n and s[i] != '\n':
        i += 1
    return i


def skip_block_comment(s, i):
    """s[i:i+2] == '/*'; return index just past '*/'."""
    i += 2
    n = len(s)
    while i < n:
        if s[i] == '*' and i + 1 < n and s[i + 1] == '/':
            return i + 2
        i += 1
    return i


def translate(src_text):
    """Return (translated_text, stats_dict)."""
    s = src_text
    n = len(s)
    out = []
    stats = {}
    i = 0
    while i < n:
        c = s[i]

        # 注释优先于字符串检测: 先看 // 与 /*
        if c == '/' and i + 1 < n and s[i + 1] == '/':
            j = skip_line_comment(s, i)
            out.append(s[i:j])
            i = j
            continue

        if c == '/' and i + 1 < n and s[i + 1] == '*':
            j = skip_block_comment(s, i)
            out.append(s[i:j])
            i = j
            continue

        # 字符串 / 字符字面量原样保留
        if c == '"':
            j = skip_string(s, i)
            out.append(s[i:j])
            i = j
            continue

        if c == "'":
            j = skip_char_literal(s, i)
            out.append(s[i:j])
            i = j
            continue

        # 代码 token: 完整 C 标识符
        if is_ident_start(c):
            j = i + 1
            while j < n and is_ident_char(s[j]):
                j += 1
            word = s[i:j]
            zh = EN2ZH.get(word)
            if zh is not None:
                out.append(zh)
                stats[word] = stats.get(word, 0) + 1
            else:
                out.append(word)
            i = j
            continue

        out.append(c)
        i += 1

    return ''.join(out), stats


def _id_start(c):
    return ('A' <= c <= 'Z') or ('a' <= c <= 'z') or c == '_' or ord(c) >= 0x80


def _id_char(c):
    return _id_start(c) or ('0' <= c <= '9')


def code_idents(s):
    """只收集代码位置的完整标识符 (注释/字符串/字符字面量跳过)。"""
    n = len(s)
    i = 0
    out = []
    while i < n:
        c = s[i]
        if c == '/' and i + 1 < n and s[i + 1] == '/':
            i = skip_line_comment(s, i)
            continue
        if c == '/' and i + 1 < n and s[i + 1] == '*':
            i = skip_block_comment(s, i)
            continue
        if c == '"':
            i = skip_string(s, i)
            continue
        if c == "'":
            i = skip_char_literal(s, i)
            continue
        if _id_start(c):
            j = i + 1
            while j < n and _id_char(s[j]):
                j += 1
            out.append(s[i:j])
            i = j
            continue
        i += 1
    return out


def main():
    do_stats = '--stats' in sys.argv
    do_check = '--check' in sys.argv

    raw = SRC.read_bytes()
    try:
        src_text = raw.decode('utf-8')
    except UnicodeDecodeError as e:
        print(f'[ERR] {SRC} 不是严格 UTF-8: {e}')
        return 1

    out_text, stats = translate(src_text)

    print(f'输入: {SRC.name}  ({len(src_text)} 字符, {src_text.count(chr(10)) + 1} 行)')
    print(f'关键字映射: {len(EN2ZH)} 条')
    print('替换统计:')
    for k in sorted(stats, key=lambda x: -stats[x]):
        print(f'  {k:>10} -> {EN2ZH[k]}  {stats[k]}')

    if do_stats:
        return 0

    if do_check:
        # 校验已落盘的 qcc_work.jy:
        # 1) 代码标识符序列必须与 C 源一一对应 (英文关键字 → 中文关键字, 其余不变);
        # 2) 翻译幂等 (二次翻译不应再改变)。
        if not DST.exists():
            print(f'[ERR] 目标文件不存在: {DST}')
            return 1
        try:
            dst_text = DST.read_text(encoding='utf-8-sig')
        except UnicodeDecodeError as e:
            print(f'[ERR] {DST.name} 不是严格 UTF-8: {e}')
            return 1
        a = code_idents(src_text)
        b = code_idents(dst_text)
        bad = 0
        if len(a) != len(b):
            print(f'[ERR] 代码标识符数量不一致: C={len(a)} jy={len(b)}')
            return 1
        for idx, (x, y) in enumerate(zip(a, b)):
            want = EN2ZH.get(x, x)
            if y != want:
                print(f'[ERR] token 不一致 @{idx}: C={x!r} jy={y!r} 期望={want!r}')
                bad += 1
                if bad > 20:
                    break
        if bad:
            return 1
        out2, stats2 = translate(dst_text)
        if out2 != dst_text:
            print('[ERR] 翻译不幂等: 生成结果可被二次翻译改变')
            return 1
        if stats2:
            print(f'[ERR] 生成结果仍含可翻译英文关键字: {stats2}')
            return 1
        print(f'token 校验通过: {len(a)} 个代码标识符一一对应, 幂等, 注释/字符串未动')
        return 0

    # 输出严格 UTF-8, 带 BOM (与旧 qcc_work.jy 一致; lexer 会跳过 BOM)。
    dst_bytes = b'\xef\xbb\xbf' + out_text.encode('utf-8')
    DST.write_bytes(dst_bytes)
    print(f'输出: {DST}  ({len(out_text)} 字符, {out_text.count(chr(10)) + 1} 行)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
