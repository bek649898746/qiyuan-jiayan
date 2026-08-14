# -*- coding: utf-8 -*-
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
src = open('merge-ort.c', encoding='utf-8', errors='replace').read()
lines = src.split('\n')
# 找参数类型序列含 commit, commit_list, object_id, repository, tree 的函数定义
for i, l in enumerate(lines, 1):
    if re.search(r'\b(struct commit|struct commit_list|struct object_id|struct repository|struct tree|struct merge_options_internal|struct merge_options)\b', l) and '(' in l and ')' not in l.split('(')[-1]:
        print(f'{i}: {l.strip()[:100]}')
    elif re.search(r'struct (commit|commit_list|object_id|repository|tree|merge_options_internal|merge_options)\s*\*', l) and '(' in l:
        print(f'{i}: {l.strip()[:100]}')
