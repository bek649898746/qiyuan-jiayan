# -*- coding: utf-8 -*-
"""Apply English→Chinese identifier mapping to qcc_work.jy → qcc_zh.jy.

Only renames identifiers in CODE positions (skips strings and comments).
Mapping file format: one "english chinese" pair per line, # for comments.
"""
import sys
from pathlib import Path

ROOT = Path(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

def load_map(path):
    m = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.split()
        if len(parts) >= 2:
            m[parts[0]] = parts[1]
    return m


def is_ident_start(c):
    return ('A' <= c <= 'Z') or ('a' <= c <= 'z') or c == '_' or ord(c) >= 0x80


def is_ident_char(c):
    return is_ident_start(c) or ('0' <= c <= '9')


def skip_num(text, i):
    """text[i] 是数字; 跳过数字字面量 (16 进制/10 进制/浮点/后缀), 返回新 i。
    必须跳过: 单字母映射 (f/F/e/l/u) 会撕碎 1f/1e6/1u/1L/0x7FFFFFFFULL 等字面量
    (2026-08-31 批2教训: 十进制分支先修, 十六进制分支漏了 ULL 后缀 → 0x7FFFFFFF剩进一)。"""
    n = len(text)
    if text[i] == '0' and i + 1 < n and text[i+1] in 'xX':
        i += 2
        while i < n and text[i] in '0123456789abcdefABCDEF': i += 1
        # hex 浮点: 0x1.8p3 / 0x1p3
        if i < n and text[i] == '.':
            i += 1
            while i < n and text[i] in '0123456789abcdefABCDEF': i += 1
        if i < n and text[i] in 'pP':
            i += 1
            if i < n and text[i] in '+-': i += 1
            while i < n and text[i] in '0123456789': i += 1
        # 后缀 f/u/l
        while i < n and text[i] in 'fFuUlL':
            i += 1
        return i
    while i < n and text[i] in '0123456789': i += 1
    if i < n and text[i] == '.':
        i += 1
        while i < n and text[i] in '0123456789': i += 1
    if i < n and text[i] in 'eEpP':
        i += 1
        if i < n and text[i] in '+-': i += 1
        while i < n and text[i] in '0123456789': i += 1
    while i < n and text[i] in 'fFuUlL':
        i += 1
    return i


def transform(text, mapping):
    n = len(text)
    out = []
    i = 0
    # skip strings/comments; rename identifiers in code
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            if j < 0:
                j = n
            out.append(text[i:j]); i = j; continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            if j < 0:
                j = n
            else:
                j += 2
            out.append(text[i:j]); i = j; continue
        if c == '"':
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2; continue
                if text[j] == '"':
                    j += 1; break
                j += 1
            out.append(text[i:j]); i = j; continue
        if c == "'":
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2; continue
                if text[j] == "'":
                    j += 1; break
                j += 1
            out.append(text[i:j]); i = j; continue
        if c.isdigit():
            j = skip_num(text, i)
            out.append(text[i:j]); i = j; continue
        if is_ident_start(c) and ('A' <= c <= 'Z' or 'a' <= c <= 'z' or c == '_'):
            j = i + 1
            while j < n and is_ident_char(text[j]):
                j += 1
            word = text[i:j]
            out.append(mapping.get(word, word))
            i = j
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def main():
    map_path = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / 'scripts' / 'zh_map.txt')
    mapping = load_map(map_path)
    src = ROOT / 'srclib_jiayan' / 'qcc_work.jy'
    dst = ROOT / 'srclib_jiayan' / 'qcc_zh.jy'
    text = src.read_bytes().decode('utf-8')
    out = transform(text, mapping)
    dst.write_bytes(out.encode('utf-8'))
    print(f'applied {len(mapping)} mappings -> {dst} ({len(out)} chars)')


if __name__ == '__main__':
    main()
