# -*- coding: utf-8 -*-
a = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test\_s_v1.asm.asm', encoding='utf-8', errors='replace').read().splitlines()
b = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test\_s_v2.asm.asm', encoding='utf-8', errors='replace').read().splitlines()

def find_blk(lines):
    for i, l in enumerate(lines):
        if l.strip() == 'blk:':
            return i
    return -1

ia = find_blk(a)
ib = find_blk(b)
print('v1 blk at', ia, ' v2 blk at', ib)

# count 符号扩展 within blk function (until next function header)
def until_next_fn(lines, start):
    i = start + 1
    while i < len(lines):
        l = lines[i]
        if l and not l.startswith(' ') and not l.startswith('.L') and l.endswith(':'):
            break
        i += 1
    return i

ea = until_next_fn(a, ia)
eb = until_next_fn(b, ib)
print('v1 blk range', ia, ea, 'lines:', ea-ia)
print('v2 blk range', ib, eb, 'lines:', eb-ib)

def count_movsxd(lines, s, e):
    return sum(1 for j in range(s, e) if '符号扩展' in lines[j])

print('v1 blk movsxd count:', count_movsxd(a, ia, ea))
print('v2 blk movsxd count:', count_movsxd(b, ib, eb))

# print v1 blk movsxd positions
print('--- v1 movsxd positions ---')
for j in range(ia, ea):
    if '符号扩展' in a[j]:
        print(' v1 line', j, ':', a[j].encode('unicode_escape').decode()[:60])
