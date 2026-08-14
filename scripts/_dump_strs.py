# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
strs = re.findall(r'"([^"]*)"', m)
for i, s in enumerate(strs[:15]):
    print(i, repr(s[:70]))
