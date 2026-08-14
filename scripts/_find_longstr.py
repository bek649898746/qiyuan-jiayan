# -*- coding: utf-8 -*-
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
for f in ['add-patch.c','object-name.c','sequencer.c','builtin/show-branch.c']:
    src = open(f, encoding='utf-8', errors='replace').read()
    best = 0; bestpos = 0; bestblock = ''
    i = 0
    while i < len(src):
        c = src[i]
        if c == "'":  # 字符字面量跳过
            j = i + 1
            while j < len(src):
                if src[j] == '\\':
                    j += 2; continue
                if src[j] == "'":
                    break
                j += 1
            i = j + 1; continue
        if c == '"':
            j = i; total = 0
            while j < len(src):
                k = j + 1
                while k < len(src):
                    if src[k] == '\\':
                        k += 2; continue
                    if src[k] == '"':
                        break
                    k += 1
                total += (k - j + 1)
                n = k + 1
                while n < len(src) and src[n] in ' \t\r\n':
                    n += 1
                if n < len(src) and src[n] == '"':
                    j = n; continue
                break
            if total > best:
                best = total; bestpos = i; bestblock = src[i:j+1]
            i = j + 1
        else:
            i += 1
    print(f, '最长拼接字面量', best, '字符 @', bestpos)
    print('  前缀:', repr(bestblock[:120]))
