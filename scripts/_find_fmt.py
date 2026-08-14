# -*- coding: utf-8 -*-
import re, sys
data = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
for i, line in enumerate(data.splitlines(), 1):
    s = line.strip()
    if ('sprintf' in s or 'printf' in s or 'fprintf' in s) and '%' in s:
        m = re.search(r'"([^"]*)"', s)
        if m:
            fmt = m.group(1)
            if any(ch in fmt for ch in 'feEgG') and '%' in fmt:
                print(i, s[:150])
