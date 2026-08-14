# -*- coding: utf-8 -*-
"""Add tll/tuns/tll_hi zero to numeric-macro substitution in host (GBK/UTF-8 mixed)."""
p = 'srclib/qcc_x86.c'
data = open(p, 'rb').read()
old = b'if (found_num) { tt[ti] = NK; tv[ti] = nvv; ti++; continue; }'
new = b'if (found_num) { tt[ti] = NK; tv[ti] = nvv; tuns[ti] = 0; tll[ti] = 0; tll_hi[ti] = 0; ti++; continue; } /* fix 2026-08-12: num-macro NK must clear tll/tuns - stale calloc junk -> spurious nll -> 2-cycle */'
print('host count:', data.count(old))
if data.count(old) == 1:
    open(p, 'wb').write(data.replace(old, new, 1))
    print('host patched')
else:
    print('host anchor issue')
