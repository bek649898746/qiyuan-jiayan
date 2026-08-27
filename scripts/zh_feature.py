# -*- coding: utf-8 -*-
"""甲言特性接入助手 (2026-08-27 提速①模板化)

用法:
  python scripts/zh_feature.py check <词>           # 检查词是否会被 blk() L5785 unknown-typedef 吞
  python scripts/zh_feature.py scaffold <词> [--kind stmt|prim|lexer] [--desc "描述"]
  python scripts/zh_feature.py verify [--feat <文件>]  # 一键验证 (gcc 编宿主 + c_to_jy + feat 实跑)
  python scripts/zh_feature.py finalize <词> <不动点8位> <提交说明>  # 更新 expected + 种子 + 提交

铁律: 每站提交前先写施工日志再归档 (铁律#26/提交纪律)
"""
import sys, os, subprocess, hashlib, shutil, datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'srclib', 'qcc_x86.c')
MIRROR = os.path.join(ROOT, 'srclib_jiayan', 'qcc_work.jy')
BUILD = os.path.join(ROOT, 'scripts', 'build.ps1')

def gbk_read(p):
    return open(p, 'rb').read().decode('gbk', 'replace')

def gbk_write(p, s):
    open(p, 'wb').write(s.encode('gbk'))

def utf8_read(p):
    return open(p, 'rb').read().decode('utf-8', 'replace')

def utf8_write(p, s):
    open(p, 'wb').write(s.encode('utf-8'))

def cmd(args, **kw):
    return subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', **kw)

def check_word(word):
    """模拟 blk() L5785: 词后跟 裸变量+;/,/[/*/= 会命中 unknown-typedef"""
    data = utf8_read(SRC)
    # 常见需排除形态: <词> x;  <词> x,  <词> x[  <词> x=  <词> *x;
    need = False
    reasons = []
    if word in ('解包', '范围', '送', '收', '夹取', '重载', '推导', '线程', '等待', '投递', '接收',
                '格式化', '区域', '生成', '下一个', '协程', '步进', '抛出', '若错', '捕获', '异步',
                '原子', '屏障', '映射', '过滤', '折叠', '交换', '求和', '求积', '反转', '排序',
                '最大', '最小', '查找', '保证', '计数', '去重', '归并'):
        need = True
        reasons.append('已知特性词 (已在 L5785 排除列表)')
    else:
        # 新词: 若后跟 裸标识符+;/, 形态则需排除
        reasons.append('新词: 若语法为 "<词> <变量>;" 或 "<词> <变量>," 则需排除')
        need = True  # 保守: 一律排除 (cost 极低)
    # 检查是否已在 L5785
    already = f'strcmp(tn[tk], "{word}") != 0' in data
    print(f'词: {word}')
    print(f'需 L5785 排除: {need} ({"; ".join(reasons)})')
    print(f'已在排除列表: {already}')
    return need, already

def scaffold(word, kind='stmt', desc=''):
    data = utf8_read(SRC)
    today = datetime.date.today().strftime('%Y-%m-%d')
    # 1. 生成 stmt/prim 分支骨架
    if kind == 'lexer':
        skel = f'''            else if (!strcmp(tn[ti], "{word}")) strcpy(tn[ti], "ENGLISH"); /* 接入 {today}: {desc} */'''
    else:
        fname = 'stmt' if kind == 'stmt' else 'prim'
        skel = f'''    if (tt[tk] == VR && !strcmp(tn[tk], "{word}")) {{ /* 接入 {today}: {desc} — TODO: AST 展开逻辑 */
        tk++; /* {word} */
        /* TODO: 解析参数 + 构造 AST 节点 */
        return Nd(5); /* 占位空块 */
    }}
'''
    # 2. L5785 排除行
    excl = f' && strcmp(tn[tk], "{word}") != 0'
    # 3. feat 测试模板
    test = f'''#include <stdio.h>

/* 甲言 {word} 接入测试 ({today}): {desc} */

int main(void) {{
    /* TODO: 测试用例 */
    printf("PASS\\n");
    return 0;
}}
'''
    # 4. 施工日志模板
    log = f'''# {today} 甲言 {word} 接入 — 第 N 站（新不动点 XXXX）
> 攻坚: 虾米  |  提交: (待填)  |  性质: **接入**

## 战果
{desc}

## 实现（2 处）
1. **{fname}() `{word}` 分支**: TODO
2. **blk() L5785 排除 `{word}`**

## 验证（三把尺子 + 特性行为）
- feat_XXX_zh 全 PASS
- 325/325 全绿 + 20/20 全绿
- 自举 XXXX（1-cycle, seed 已重推）
- H1==H2 现编现验 = XXXX

## 文件
- 修改: srclib/qcc_x86.c + srclib_jiayan/qcc_work.jy + scripts/build.ps1
- 测试: scripts/feat_XXX_zh.c
- 封存: build.ps1 $expected='xxxx', 种子=XXXX
'''
    print('=' * 60)
    print(f'① stmt/prim 分支骨架 ({kind}):')
    print(skel)
    print(f'② L5785 排除行 (加在 existing 排除链后):')
    print(excl)
    print(f'③ feat 测试模板 (scripts/feat_{word}_zh.c):')
    print(test)
    print(f'④ 施工日志模板 (memory/construction/):')
    print(log)
    print('=' * 60)
    print('提示: 填入 AST 逻辑后, 用 "python scripts/zh_feature.py verify" 一键验证')

def verify(feat=None):
    print('[1/3] gcc 编宿主 + c_to_jy ...')
    r = cmd(['gcc', '-O2', '-Wall', '-Werror', SRC, '-o', os.path.join(ROOT, 'qcc_x86.exe')])
    if r.returncode != 0:
        print('GCC FAIL:', r.stderr[-500:])
        return 1
    r2 = cmd(['python', os.path.join(ROOT, 'scripts', 'c_to_jy.py')])
    print('c_to_jy rc=', r2.returncode)
    if not feat:
        import glob
        feats = sorted(glob.glob(os.path.join(ROOT, 'scripts', 'feat_*.c')),
                       key=os.path.getmtime, reverse=True)
        feat = feats[0] if feats else None
    if feat:
        print(f'[2/3] 编译+实跑 {os.path.basename(feat)} ...')
        out = os.path.join(ROOT, 'ft_t.exe')
        r3 = cmd(['qcc_x86.exe', feat, '-o', out])
        if r3.returncode != 0:
            print('COMPILE FAIL:', (r3.stderr or '')[-400:])
            return 1
        p = subprocess.run(['cmd', '/c', out], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=15)
        print(f'RUN rc={p.returncode} OUT={p.stdout!r}')
        if os.path.exists(out):
            os.remove(out)
    print('[3/3] 完整验证: 跑 build.ps1 (五代全等 + 325/325 + 20/20) ...')
    r4 = cmd(['powershell', '-Command', './scripts/build.ps1'])
    print('build.ps1 rc=', r4.returncode)
    if r4.returncode == 0:
        # H1==H2
        r5 = cmd(['gcc', '-O2', '-Os', '-s', SRC, '-o', os.path.join(ROOT, 'h1v.exe')])
        r6 = cmd([os.path.join(ROOT, 'h1v.exe'), MIRROR, '-o', os.path.join(ROOT, 'h1o.exe')])
        if r6.returncode == 0:
            h = hashlib.sha256(open(os.path.join(ROOT, 'h1o.exe'), 'rb').read()).hexdigest()[:8].upper()
            exp = gbk_read(BUILD).split("expected = '")[1][:8]
            print(f'H1={h} expected={exp} H1==H2: {h.lower()==exp}')
            os.remove(os.path.join(ROOT, 'h1v.exe'))
            os.remove(os.path.join(ROOT, 'h1o.exe'))
    return r4.returncode

def finalize(word, fix, msg):
    """更新 expected + 重推种子 + 提交"""
    # 1. expected
    b = gbk_read(BUILD)
    import re
    m = re.search(r"expected = '([0-9a-f]{8})'", b)
    old = m.group(1)
    b = b.replace(f"expected = '{old}'", f"expected = '{fix}'")
    gbk_write(BUILD, b)
    print(f'expected {old} → {fix}')
    # 2. 种子: v1.exe → qcc_boot.exe
    v1 = os.path.join(ROOT, 'v1.exe')
    if os.path.exists(v1):
        h = hashlib.sha256(open(v1, 'rb').read()).hexdigest()[:8].upper()
        if h == fix.upper():
            shutil.copy(v1, os.path.join(ROOT, 'qcc_boot.exe'))
            print('seed 已重推:', h)
    # 3. 提交
    import glob
    feats = ' '.join(f'scripts/{os.path.basename(f)}' for f in
                     sorted(glob.glob(os.path.join(ROOT, 'scripts', 'feat_*.c')),
                            key=os.path.getmtime, reverse=True)[:2])
    os.chdir(ROOT)
    r = cmd(['git', 'add', 'scripts/build.ps1', 'srclib/qcc_x86.c', 'srclib_jiayan/qcc_work.jy'] + feats.split())
    msg_file = os.path.join(ROOT, '_commit_msg.txt')
    utf8_write(msg_file, msg)
    r2 = cmd(['git', 'commit', '-F', msg_file])
    print('commit rc=', r2.returncode)
    if os.path.exists(msg_file):
        os.remove(msg_file)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    op = sys.argv[1]
    if op == 'check' and len(sys.argv) >= 3:
        check_word(sys.argv[2])
    elif op == 'scaffold' and len(sys.argv) >= 3:
        kind = 'stmt'
        desc = ''
        if '--kind' in sys.argv:
            kind = sys.argv[sys.argv.index('--kind') + 1]
        if '--desc' in sys.argv:
            desc = sys.argv[sys.argv.index('--desc') + 1]
        scaffold(sys.argv[2], kind, desc)
    elif op == 'verify':
        feat = None
        if '--feat' in sys.argv:
            feat = sys.argv[sys.argv.index('--feat') + 1]
        sys.exit(verify(feat))
    elif op == 'finalize' and len(sys.argv) >= 5:
        finalize(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        print(__doc__)
        sys.exit(1)
