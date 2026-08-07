# -*- coding: utf-8 -*-
"""Temporary runner: run run_tests.py logic with a custom compiler exe."""
import sys, os
sys.stdout.reconfigure(encoding='utf-8')

qcc_name = sys.argv[1] if len(sys.argv) > 1 else 'v1.exe'
src = open(os.path.join('scripts', 'run_tests.py'), 'rb').read().decode('utf-8')
src = src.replace("qcc_x86.exe", qcc_name)
src = src.replace("qcc = os.path.join(root, '" + qcc_name + "')",
                  "qcc = os.path.join(root, '" + qcc_name + "')")
ns = {'__file__': os.path.join('scripts', 'run_tests.py')}
exec(compile(src, 'run_tests_custom.py', 'exec'), ns)
