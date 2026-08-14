# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
strs = re.findall(r'"([^"]*)"', m)
print('total string literals:', len(strs))
enc_names = {'rax','rcx','rdx','rbx','rsp','rbp','rsi','rdi','eax','ecx','edx','ebx','esp','ebp','esi','edi','al','cl','dl','bl','spl','bpl','sil','dil','r8','r9','r10','r11','r12','r13','r14','r15','r8d','r9d','r10d','r11d','r12d','r13d','r14d','r15d','hlt','nop','ret','retq','sti','cli','int3','iretq','push','pop','mov','add','sub','cmp','%'}
enc = [s for s in strs if s in enc_names]
print('encoder-related strings:', len(enc))
