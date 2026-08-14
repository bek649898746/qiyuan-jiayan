# -*- coding: utf-8 -*-
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
n = len(re.findall(r'"', m))
print('quote count:', n, '~strings:', n // 2)
