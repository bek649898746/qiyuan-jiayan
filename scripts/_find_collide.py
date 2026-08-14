# -*- coding: utf-8 -*-
"""Find collisions between function-local statics and globals in the mirror.
A codegen lookup for name N during function F scans [0, fve[F]): an EARLIER
function's local static named N shadows the global N (the BLOCKER-3 leak).

Heuristic: find `静 <type> <name>` declarations inside function bodies, and
file-scope `静 <type> <name>` globals; report names in both sets.
"""
import re

raw = open(r'srclib_jiayan\qcc_work.jy', 'rb').read()
txt = raw.decode('utf-8')
lines = txt.splitlines()

TYPE = r'(?:整|字|浮|长|构|空)'
# static decl: 静 <type> <name>  (possibly 静 构 { ... } name; / 静 <type> *name / name[...])
static_re = re.compile(r'^\s*静\s+(?:(?:无|有)符号\s+)?' + TYPE + r'\s*(\*?\s*)([A-Za-z_][A-Za-z0-9_]*)\s*[;=\[\.]')

# find function body ranges: 静 整 name( ... ) {   or  整 name(...) {   (top-level)
fn_re = re.compile(r'^\s*(?:静\s+)?(?:整|空|字|浮)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(')

def find_body_ranges():
    ranges = []  # (name, start_line, end_line)
    brace_depth = 0
    cur = None
    for i, l in enumerate(lines):
        if cur is None:
            m = fn_re.match(l)
            if m:
                # look for '{' on this line or the start of function
                cur = m.group(1)
                brace_depth = 0
        if cur is not None:
            brace_depth += l.count('{') - l.count('}')
            if brace_depth <= 0 and cur is not None and '{' in ''.join(lines[max(0,i-1):i+1]):
                pass
            if brace_depth <= 0:
                ranges.append((cur, i))
                cur = None
    return ranges

ranges = find_body_ranges()

def collect_statics(lo, hi):
    out = set()
    for i in range(lo, hi):
        m = static_re.match(lines[i])
        if m:
            out.add(m.group(2))
    return out

fn_statics = {}
all_fn_static = set()
for name, end in ranges:
    fn_statics[name] = collect_statics(0, end)  # rough: up to end of this fn
    all_fn_static |= collect_statics(0, end)

# globals: statics declared before the first function definition, plus 静 at file scope
first_fn = ranges[0][1] if ranges else len(lines)
globals_set = set()
for i in range(0, first_fn):
    m = static_re.match(lines[i])
    if m:
        globals_set.add(m.group(2))

print('num functions:', len(ranges))
print('num global statics:', len(globals_set))
print('num fn-local statics (cumulative):', len(all_fn_static))

# collision: a name that is a fn-local static in ANY function AND a global/extern
collisions = all_fn_static & globals_set
print('=== GLOBAL collisions ===')
for n in sorted(collisions):
    print(' ', n)

# also: fn-local statics in an EARLIER function colliding with a LATER function's
# non-static local (reference in later fn) — the leak makes later fn's refs find the earlier static
# (harder to detect without full ref analysis; report fn-local static names that repeat across fns)
from collections import defaultdict
name_fns = defaultdict(list)
for name, end in ranges:
    for s in collect_statics(max(0, end-400), end):  # crude: statics in last 400 lines of each fn
        name_fns[s].append(name)
print('=== fn-local statics used in multiple functions ===')
for n, fns in sorted(name_fns.items()):
    if len(fns) > 1:
        print(' ', n, '<-', fns)
