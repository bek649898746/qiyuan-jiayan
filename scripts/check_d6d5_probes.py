# -*- coding: utf-8 -*-
"""check_d6d5_probes —— D6D5 五处修复的**最小复现**（修复前红 / 修复后绿，真跑）

做法：从 git 取**修复前**源码，用当前编译器编出旧工具，与现役工具在同一探针上对拍。
  旧优化器 `4cdefe44:srclib/opt_zh.jy`（⑧/⑪/⑫/⑯ 修复前）
  旧 ELF：`qcc_zh_elf.exe`（实四 初版，无 Linux 垫片）或 `qcc_zh_elf4.exe`（垫片 + 32 位增量）
          两者本机 WSL 均段错误；现役 `qcc_zh_v2.exe` 输出 hello、rc=0。

探针：
  08_alias   移动 r0,148 + 零扩展 eax,al      旧: 删掉 mov → rc=0；新: 保留 → rc=148
  11_width   负 32 位比较（A==r0）             旧: 64 位比较 → rc=0；新: 比较32 → rc=1
  12_operand 算术融合后 rX 仍被读               旧: rX 未写 → rc=9；新: rc=2
  16_imm     移动 r0,5                         旧: .text 有效多 2B；新: 压栈/弹栈（两者 rc=5）
  blockshadow 块内同名声明遮蔽形参后读外层          旧: f=5 / 指针 0xC0000005；新: 42 42 42
  elf64      ELF hello                        旧: WSL 段错误；新: hello, rc=0
  declorder  同块后声明遮蔽前引用                旧: 0 0；新: 7 42

退出码: 0 = 全部「旧红/新绿」成立；1 = 不成立；2 = 环境未就绪（git/编译器/WSL）
"""
import io, os, struct, subprocess, sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
sys.path.insert(0, os.path.join(ROOT, 'scripts'))
try:
    from qcc_tmp import tmp_dir
except Exception as e:
    print('[FAIL] 缺 qcc_tmp: %s' % e)
    sys.exit(2)

NEW_OPT = 'opt_zh.exe'
NEW_CC = 'qcc_zh_v2.exe'
OLD_OPT_SRC = '4cdefe44:srclib/opt_zh.jy'
W = os.path.join(tmp_dir('qcc_d6d5_probes'), 'run')
os.makedirs(W, exist_ok=True)
fails = []


def sh(a, t=1800):
    return subprocess.run(a, cwd=ROOT, capture_output=True, timeout=t)


def text_eff(path):
    d = io.open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    o = struct.unpack_from('<H', d, pe + 20)[0]
    q0 = pe + 24 + o
    for i in range(n):
        q = q0 + 40 * i
        if d[q:q + 8].rstrip(b'\x00').decode('latin1') == '.text':
            rsz, ro = struct.unpack_from('<II', d, q + 16)
            body = d[ro:ro + rsz]
            j = len(body) - 1
            while j >= 0 and body[j] == 0:
                j -= 1
            return j + 1
    return -1


def build_old_opt():
    r = sh(['git', 'show', OLD_OPT_SRC])
    if r.returncode != 0:
        return None
    p = os.path.join(W, 'opt_old.jy')
    open(p, 'wb').write(r.stdout)
    out = os.path.join(W, 'opt_old.exe')
    r = sh([NEW_CC, p, '-o', out])
    if r.returncode != 0:
        print('[FAIL] 旧优化器编译失败: %s' % (r.stdout + r.stderr).decode('utf-8', 'replace')[-200:])
        return None
    return out


def chain(exe_opt, asm_text, tag):
    p = os.path.join(W, tag + '.asm')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(asm_text)
    o = os.path.join(W, tag + '_o.asm')
    r = sh([exe_opt, p, '-o', o])
    opt_log = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()
    if r.returncode != 0:
        return None, opt_log, None
    e = os.path.join(W, tag + '.exe')
    r = sh(['asm_zh.exe', o, '-o', e])
    if r.returncode != 0:
        return None, opt_log, None
    r = sh([e])
    return r.returncode, opt_log, text_eff(e)


def wsl_run(p):
    wp = '/mnt/' + p[0].lower() + p[2:].replace('\\', '/')
    r = subprocess.run(['wsl', '-d', 'Ubuntu', '--', 'bash', '-c',
                        'chmod +x "%s" && "%s"' % (wp, wp)], capture_output=True, timeout=300)
    return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()


def main():
    if not (os.path.exists(NEW_OPT) and os.path.exists(NEW_CC)):
        print('[FAIL] 现役 opt_zh.exe / qcc_zh_v2.exe 缺失')
        return 2
    old_opt = build_old_opt()
    if old_opt is None:
        return 2

    # 08
    asm08 = '_入口:\n    移动 r0, 148\n    零扩展 eax, al\n    返回\n'
    a = chain(old_opt, asm08, 'p08_old')
    b = chain(NEW_OPT, asm08, 'p08_new')
    ok = (a[0] == 0 and b[0] == 148)
    print('  08_alias   旧 rc=%s 新 rc=%s  命中(旧)=%s  %s'
          % (a[0], b[0], '死值写覆盖 1' in a[1], 'OK' if ok else 'FAIL'))
    if not ok:
        fails.append('08_alias 旧红/新绿不成立')

    # 11
    asm11 = ('_入口:\n    移动 r0, -1\n    压栈 r0\n    移动 r0, 0\n    弹栈 r3\n'
             '    比较 r3, r0\n    置小 al\n    零扩展 eax, al\n    测试 r0, r0\n'
             '    为零跳 .Lz\n    移动 r0, 1\n    返回\n.Lz:\n    移动 r0, 0\n    返回\n')
    a = chain(old_opt, asm11, 'p11_old')
    b = chain(NEW_OPT, asm11, 'p11_new')
    ok = (a[0] == 0 and b[0] == 1)
    print('  11_width   旧 rc=%s 新 rc=%s  %s' % (a[0], b[0], 'OK' if ok else 'FAIL'))
    if not ok:
        fails.append('11_width 旧红/新绿不成立')

    # 12（rX=r6 在融合后仍被读）
    asm12 = ('_入口:\n    移动 r6, 9\n    移动 r0, 7\n    压栈 r0\n    移动 r0, 5\n'
             '    弹栈 r6\n    减 r6, r0\n    移32 r0, r6\n    移32 r0, r6\n    返回\n')
    a = chain(old_opt, asm12, 'p12_old')
    b = chain(NEW_OPT, asm12, 'p12_new')
    ok = (a[0] == 9 and b[0] == 2)
    print('  12_operand 旧 rc=%s 新 rc=%s  %s' % (a[0], b[0], 'OK' if ok else 'FAIL'))
    if not ok:
        fails.append('12_operand 旧红/新绿不成立')

    # 16（比较 .text 有效长度）
    asm16 = '_入口:\n    移动 r0, 5\n    返回\n'
    a = chain(old_opt, asm16, 'p16_old')
    b = chain(NEW_OPT, asm16, 'p16_new')
    ok = (a[0] == 5 and b[0] == 5 and a[2] is not None and b[2] is not None and b[2] == a[2] - 2)
    print('  16_imm     旧 rc=%s/%sB 新 rc=%s/%sB  %s'
          % (a[0], a[2], b[0], b[2], 'OK' if ok else 'FAIL'))
    if not ok:
        fails.append('16_imm 旧红/新绿不成立')

    # elf64
    hello = os.path.join(W, 'hello.c')
    io.open(hello, 'w', encoding='utf-8', newline='\n').write(
        '#include <stdio.h>\nint main(void) { printf("hello\\n"); return 0; }\n')
    new_elf = os.path.join(W, 'new.elf')
    rn = sh([NEW_CC, '--target=elf', hello, '-o', new_elf])
    old_bin = next((x for x in ('qcc_zh_elf.exe', 'qcc_zh_elf4.exe') if os.path.exists(x)), None)
    if rn.returncode != 0:
        fails.append('现役 ELF 产物生成失败')
    if old_bin is None:
        print('  elf64      旧二进制缺失（qcc_zh_elf.exe/qcc_zh_elf4.exe）⟹ 仅证据：'
              '审计复核《_通知_本机已有Linux环境》记录的段错误')
    else:
        old_elf = os.path.join(W, 'old.elf')
        ro = sh([old_bin, '--target=elf', hello, '-o', old_elf])
        if ro.returncode != 0:
            fails.append('旧 ELF 产物生成失败')
        else:
            a = wsl_run(old_elf)
            b = wsl_run(new_elf)
            ok = (a[0] != 0 and b[0] == 0 and b[1] == 'hello')
            print('  elf64      旧(%s) rc=%s 新 rc=%s out=%r  %s'
                  % (old_bin, a[0], b[0], b[1], 'OK' if ok else 'FAIL'))
            if not ok:
                fails.append('elf64 旧红/新绿不成立')

    # 6) blockshadow —— 从 git 取修复前主源，用现役编译器重建「修复前编译器」
    #    复现：形参被块内同名声明遮蔽后块外读（int → 5；指针 → 0xC0000005），
    #    以及 `整 id(双 x){ return x; }` 返回残留寄存器（缺 cvttsd2si）。
    OLD_CC_SRC = '198b8ea3:srclib_jiayan/qcc_zh.jy'
    shadow_c = os.path.join(W, 'shadow_probe.c')
    io.open(shadow_c, 'w', encoding='utf-8', newline='\n').write(
        'int printf(const char *fmt, ...);\n'
        'int f(int x) { { int x = 5; (void)x; } return x; }\n'
        'int g(int *p) { { int *p = 0; (void)p; } return *p; }\n'
        'int h(double x) { { double x = 5; (void)x; } return x; }\n'
        'int main(void) { int v = 42; printf("%d %d %d\\n", f(42), g(&v), h(42)); return 0; }\n')

    def run_shadow(cc, tag):
        e = os.path.join(W, 'shadow_' + tag + '.exe')
        r = sh([cc, shadow_c, '-o', e], 900)
        if r.returncode != 0:
            return None, ''
        r = sh([e], 120)
        return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()

    r = sh(['git', 'show', OLD_CC_SRC], 120)
    if r.returncode != 0:
        fails.append('blockshadow 取修复前主源失败')
    else:
        old_src = os.path.join(W, 'qcc_old_shadow.jy')
        io.open(old_src, 'wb').write(r.stdout)
        old_cc = os.path.join(W, 'qcc_old_shadow.exe')
        rb = sh([NEW_CC, old_src, '-o', old_cc], 1800)
        if rb.returncode != 0:
            fails.append('blockshadow 修复前编译器构建失败')
        else:
            a = run_shadow(old_cc, 'old')
            b = run_shadow(NEW_CC, 'new')
            ok = (a[0] != 0 or a[1] != '42 42 42') and b[0] == 0 and b[1] == '42 42 42'
            print('  blockshadow 旧 rc=%s out=%r 新 rc=%s out=%r  %s'
                  % (a[0], a[1], b[0], b[1], 'OK' if ok else 'FAIL'))
            if not ok:
                fails.append('blockshadow 旧红/新绿不成立')

    # 7) declorder —— 声明顺序作用域（C11 6.2.1p7：作用域自声明符之后开始）
    #    旧: 同块内后声明的同名变量遮蔽此前引用（global=0 / param=0）；新: 7 42。
    OLD_CC_SRC_D7 = '19185f13:srclib_jiayan/qcc_zh.jy'  # 修复前 主源 (固定 提交, 不随 HEAD 漂移)
    decl_c = os.path.join(W, 'decl_order_probe.c')
    io.open(decl_c, 'w', encoding='utf-8', newline='\n').write(
        'int printf(const char *fmt, ...);\n'
        'int gv = 7;\n'
        'int f(void) { int y = gv; int gv = 5; (void)gv; return y; }\n'
        'int g(int x) { { int y = x; int x = 5; (void)x; return y; } }\n'
        'int main(void) { printf("%d %d\\n", f(), g(42)); return 0; }\n')

    def run_decl(cc, tag):
        e = os.path.join(W, 'decl_' + tag + '.exe')
        r = sh([cc, decl_c, '-o', e], 900)
        if r.returncode != 0:
            return None, ''
        r = sh([e], 120)
        return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()

    r = sh(['git', 'show', OLD_CC_SRC_D7], 120)
    if r.returncode != 0:
        fails.append('declorder 取修复前主源失败')
    else:
        old_src_d7 = os.path.join(W, 'qcc_old_decl.jy')
        io.open(old_src_d7, 'wb').write(r.stdout)
        old_cc_d7 = os.path.join(W, 'qcc_old_decl.exe')
        rb = sh([NEW_CC, old_src_d7, '-o', old_cc_d7], 1800)
        if rb.returncode != 0:
            fails.append('declorder 修复前编译器构建失败')
        else:
            a = run_decl(old_cc_d7, 'old')
            b = run_decl(NEW_CC, 'new')
            ok = (a[0] != 0 or a[1] != '7 42') and b[0] == 0 and b[1] == '7 42'
            print('  declorder  旧: rc=%s out=%r -> rc=%s out=%r  %s'
                  % (a[0], a[1], b[0], b[1], 'OK' if ok else 'FAIL'))
            if not ok:
                fails.append('declorder 旧红/新绿不成立')

    print('== 结果: %s ==' % ('PASS（七组旧红/新绿）' if not fails else 'FAIL=%d' % len(fails)))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
