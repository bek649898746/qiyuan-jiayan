# -*- coding: utf-8 -*-
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
src = open(r'C:\Users\Administrator\Desktop\git-2.45.2\_fn_out.txt', encoding='utf-8', errors='replace').read()
import re
ms = list(re.finditer(r'RE_LIMITED_OPS', src))
for m in ms:
    i = m.start()
    prev5 = src[max(0,i-6):i]
    print(f'@{i}: prev5={prev5!r} hex={[hex(ord(c)) for c in prev5]}')
