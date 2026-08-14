# -*- coding: utf-8 -*-
"""Find the first STRUCTURAL divergence between two .asm.asm mnemonics files.
Normalizes [rip+N] offsets and .Lxxxx labels, then finds first differing line.
Usage: python _diff_struct.py <fileA> <fileB> [max_ctx]
"""
import re, sys

def norm_lines(path):
    raw = open(path, 'rb').read()
    for enc in ('utf-8', 'gbk'):
        try:
            txt = raw.decode(enc)
            break
        except UnicodeDecodeError:
            continue
    lines = txt.splitlines()
    out = []
    for l in lines:
        l2 = re.sub(r'\[rip\+[0-9]+\]', '[rip+X]', l)
        l2 = re.sub(r'\.L[0-9]+', '.Lx', l2)
        out.append(l2)
    return out

def main():
    a = norm_lines(sys.argv[1])
    b = norm_lines(sys.argv[2])
    print('lines:', len(a), len(b))
    max_ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 14
    i = 0
    while i < min(len(a), len(b)):
        if a[i] != b[i]:
            break
        i += 1
    print('first structural diff at line', i)
    print('=== A ===')
    for j in range(max(0, i - 8), min(len(a), i + max_ctx)):
        print(j, a[j][:100])
    print('=== B ===')
    for j in range(max(0, i - 8), min(len(b), i + max_ctx)):
        print(j, b[j][:100])

if __name__ == '__main__':
    main()
