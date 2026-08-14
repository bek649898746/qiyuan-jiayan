# -*- coding: utf-8 -*-
lines = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test\_sv1b.asm.asm', encoding='utf-8', errors='replace').read().splitlines()
for j in range(131070, 131110):
    print(j, lines[j].encode('unicode_escape').decode()[:78])
