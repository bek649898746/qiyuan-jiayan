# -*- coding: utf-8 -*-
# estimate slot layout for v1.exe (.data base 0x4f2000, DATA_RVA_OFF 0x300)
base = 0x4f2000 + 0x300
slot = 0
def reg(name, slots):
    global slot
    print('%s: slot %d addr 0x%x' % (name, slot, base + 4*slot))
    slot += slots

reg('code', 2)          # pointer (8 bytes)
reg('cp,cc', 2)
reg('asm_out', 2)       # pointer
reg('asm_pass,asm_mode', 2)
reg('lc,epi,brk,cont', 4)
reg('__pad0[0x300000]', 0x300000//4)
reg('str_tbl[2048][2048]', 2048*2048//4)
reg('str_cnt', 1)
reg('vars[4096]', 4096*26)
reg('var_static_kw[4096]', 4096)
reg('vcnt', 1)
reg('stc_n', 1)
reg('fdef_list[1024]', 1024)
reg('fdef_n', 1)
reg('root_global..', 6)
reg('fvb[512],fve[512]', 1024)
reg('fr_start[512],fr_end[512]', 1024)
reg('gen_final,gfn,vs_end,parse_base', 4)
reg('cg_* 10 globals', 10)
reg('cur_fn_sret..', 4)
reg('macros[2048] (36B)', 2048*9)
reg('macro_n', 1)
reg('str_macros[2048] (2080B)', 2048*520)
reg('str_macro_n', 1)
reg('fn_macros[1024] (~1072B)', 1024*268)
reg('fn_macro_n', 1)
reg('pp_guard[128][32]', 128*8)
reg('pp_guard_val[128],pp_guard_n', 129)
print('--- token arrays (nll near here) ---')
reg('tt ptr (tokens)', 2)
reg('tv ptr', 2)
reg('tuns ptr', 2)
reg('tll ptr', 2)
reg('tll_hi ptr', 2)
