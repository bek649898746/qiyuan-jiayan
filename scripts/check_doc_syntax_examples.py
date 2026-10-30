# -*- coding: utf-8 -*-
"""check_doc_syntax_examples —— 语法文档里的示例必须**真能编译运行**（ORD-31 验收）。

做法（不是手抄）：从 `manual/中文关键字表.md` 的「五之二」一节里**抽取**围栏代码块，
按文档顺序拼成可执行程序，用仓库自带编译器编译并运行，断言输出。

覆盖：
  ① `判别` 声明 + tag 常量 + 载荷字段名（块含 `判别 结局 于`）
  ② `依据` 穷尽分派（块含 `依据`）—— 断言第一个参数被赋成命中臂的值
  ③ `解构`（块含 `解构`）—— 断言打印出 tag 与载荷值

用法: python scripts/check_doc_syntax_examples.py [--cc qcc_zh_v1.exe]
退出: 0 = 三段示例都编过且输出符合；1 = 有红
"""
import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(REPO, 'manual', '中文关键字表.md')


def blocks_after_section(text, header='五之二'):
    i = text.find(header)
    if i < 0:
        return []
    seg = text[i:]
    return [m.group(1) for m in re.finditer(r'```c\n(.*?)```', seg, re.S)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    doc = io.open(os.environ.get('QCC_DOC_MANUAL') or DOC, encoding='utf-8').read()
    bs = blocks_after_section(doc)
    blk_disc = next((b for b in bs if '判别 结局 于' in b), None)
    blk_yiju = next((b for b in bs if b.strip().startswith('整 分支计') or '穷尽 结局' in b), None)
    blk_jg = next((b for b in bs if '解构(' in b), None)
    w = tempfile.mkdtemp(prefix='qcc_doc_syntax_')
    fails = []
    print('=== 语法文档示例真跑（manual/中文关键字表.md §五之二）===')
    print('抽到代码块 %d，判别=%s 依据=%s 解构=%s' %
          (len(bs), bool(blk_disc), bool(blk_yiju), bool(blk_jg)))

    def run(name, src, expect):
        p = os.path.join(w, name + '.c')
        exe = os.path.join(w, name + '.exe')
        io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
        r = subprocess.run([cc, p, '-o', exe], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=180)
        if r.returncode != 0:
            msg = ((r.stdout or '') + (r.stderr or '')).strip().split('\n')[0][:160]
            print('  %-8s 编译失败 rc=%d %s' % (name, r.returncode, msg))
            fails.append(name + ' 编不过')
            return
        q = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        out = (q.stdout or '').strip()
        ok = expect in out
        print('  %-8s rc=%d out=%r 期望含 %r -> %s' % (name, q.returncode, out[:80], expect, ok))
        if not ok:
            fails.append(name + ' 输出不符')

    if blk_disc and blk_yiju:
        prog = ('#include <stdio.h>\nint main(void) {\n' + blk_disc + '\n' + blk_yiju +
                '\n    格式化 "D1 %d\\n", 分支计;\n    返 0;\n}\n')
        run('disc_yiju', prog, 'D1 1')       # 果.tag = 结局_成 ⟹ 命中 `结局_成: 1`
    else:
        fails.append('文档里找不到 判别/依据 代码块')

    if blk_jg:
        # 手册里的 解构 示例是**完整程序**（含 求长 与 主），按整程序编译
        prog = blk_jg if '主(空)' in blk_jg else ('#include <stdio.h>\nint main(void) {\n' + blk_disc + '\n' + blk_jg + '\n    返 0;\n}\n')
        run('jiegou', prog, 'D2 v=6 t=0')
    else:
        fails.append('文档里找不到 解构 代码块')

    print('=== 语法文档示例: %s ===' % ('PASS' if not fails else 'FAIL %s' % fails))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
