# -*- coding: utf-8 -*-
import re
for p, enc in [('srclib_jiayan/qcc_work.jy','utf-8'), ('srclib/qcc_x86.c','gbk')]:
    data = open(p,'rb').read().decode(enc, errors='replace')
    print('===', p, '===')
    for i, line in enumerate(data.splitlines(), 1):
        s = line.strip()
        if '%' in s and any(x in s for x in ['sprintf','printf','fprintf','snprintf']):
            m = re.search(r'"([^"]*)"', s)
            if m:
                fmt = m.group(1)
                if any(ch in fmt for ch in 'feEgG'):
                    print(i, s[:160])
