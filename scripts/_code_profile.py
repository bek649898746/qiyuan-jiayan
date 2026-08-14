# -*- coding: utf-8 -*-
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
for f in ['merge-ort.c', 'sequencer.c']:
    src = open(f, encoding='utf-8', errors='replace').read()
    print('===', f, '===')
    for k, r in [('#define', r'#define'), ('#if', r'#if'), ('switch', r'\bswitch\b'),
                 ('case', r'\bcase\b'), ('struct', r'\bstruct\b'), ('static', r'\bstatic\b'),
                 ('goto', r'\bgoto\b'), ('string lit', r'"'), ('enum', r'\benum\b'),
                 ('union', r'\bunion\b'), ('typeof', r'\btypeof\b'), ('goto label', r'^[a-z_]+:$'),
                 ('## macro', r'##'), ('__LINE__', r'__LINE__')]:
        print('  %-12s %d' % (k, len(re.findall(r, src, re.M))))
