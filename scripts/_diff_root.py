# -*- coding: utf-8 -*-
"""Find the FIRST divergence between two .asm.asm mnemonics files.
Usage: python _diff_root.py <fileA> <fileB>
Prints the first N differing lines with context from both files.
"""
import sys

def read_lines(path):
    with open(path, 'rb') as f:
        data = f.read()
    return data.decode('latin-1').split('\n')

def main():
    a = read_lines(sys.argv[1])
    b = read_lines(sys.argv[2])
    n = min(len(a), len(b))
    diffs = 0
    shown = 0
    for i in range(n):
        if a[i] != b[i]:
            diffs += 1
            if shown < 20:
                shown += 1
                lo = max(0, i - 3)
                print(f"=== first diff at line {i+1} (0-based {i}) ===")
                for j in range(lo, min(n, i + 4)):
                    mark = '>>>' if j == i else '   '
                    print(f"{mark} A[{j+1}]: {a[j].rstrip()}")
                for j in range(lo, min(n, i + 4)):
                    mark = '>>>' if j == i else '   '
                    print(f"{mark} B[{j+1}]: {b[j].rstrip()}")
                print()
            if diffs >= 200:
                print(f"... ({diffs} diff lines so far, stopping)")
                break
    print(f"total diff lines (first {diffs} shown up to 200): {diffs}")
    print(f"A lines: {len(a)}, B lines: {len(b)}")

if __name__ == '__main__':
    main()
