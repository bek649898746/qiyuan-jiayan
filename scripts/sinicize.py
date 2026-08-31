# -*- coding: utf-8 -*-
"""Apply English→Chinese identifier mapping to qcc_work.jy → qcc_zh.jy.

Only renames identifiers in CODE positions (skips strings and comments).
Mapping file format: one "english chinese" pair per line, # for comments.
"""
import sys
from pathlib import Path

ROOT = Path(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

def load_map(path):
    m = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.split()
        if len(parts) >= 2:
            m[parts[0]] = parts[1]
    return m


def is_ident_start(c):
    return ('A' <= c <= 'Z') or ('a' <= c <= 'z') or c == '_' or ord(c) >= 0x80


def is_ident_char(c):
    return is_ident_start(c) or ('0' <= c <= '9')


def skip_num(text, i):
    """text[i] 是数字; 跳过数字字面量 (16 进制/10 进制/浮点/后缀), 返回新 i。
    必须跳过: 单字母映射 (f/F/e/l/u) 会撕碎 1f/1e6/1u/1L/0x7FFFFFFFULL 等字面量
    (2026-08-31 批2教训: 十进制分支先修, 十六进制分支漏了 ULL 后缀 → 0x7FFFFFFF剩进一)。"""
    n = len(text)
    if text[i] == '0' and i + 1 < n and text[i+1] in 'xX':
        i += 2
        while i < n and text[i] in '0123456789abcdefABCDEF': i += 1
        # hex 浮点: 0x1.8p3 / 0x1p3
        if i < n and text[i] == '.':
            i += 1
            while i < n and text[i] in '0123456789abcdefABCDEF': i += 1
        if i < n and text[i] in 'pP':
            i += 1
            if i < n and text[i] in '+-': i += 1
            while i < n and text[i] in '0123456789': i += 1
        # 后缀 f/u/l
        while i < n and text[i] in 'fFuUlL':
            i += 1
        return i
    while i < n and text[i] in '0123456789': i += 1
    if i < n and text[i] == '.':
        i += 1
        while i < n and text[i] in '0123456789': i += 1
    if i < n and text[i] in 'eEpP':
        i += 1
        if i < n and text[i] in '+-': i += 1
        while i < n and text[i] in '0123456789': i += 1
    while i < n and text[i] in 'fFuUlL':
        i += 1
    return i


def transform(text, mapping):
    n = len(text)
    out = []
    i = 0
    # skip strings/comments; rename identifiers in code
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            if j < 0:
                j = n
            out.append(text[i:j]); i = j; continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            if j < 0:
                j = n
            else:
                j += 2
            out.append(text[i:j]); i = j; continue
        if c == '"':
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2; continue
                if text[j] == '"':
                    j += 1; break
                j += 1
            out.append(text[i:j]); i = j; continue
        if c == "'":
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2; continue
                if text[j] == "'":
                    j += 1; break
                j += 1
            out.append(text[i:j]); i = j; continue
        if c.isdigit():
            j = skip_num(text, i)
            out.append(text[i:j]); i = j; continue
        if is_ident_start(c) and ('A' <= c <= 'Z' or 'a' <= c <= 'z' or c == '_'):
            j = i + 1
            while j < n and is_ident_char(text[j]):
                j += 1
            word = text[i:j]
            out.append(mapping.get(word, word))
            i = j
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def post_process(out):
    """批3b/3c 后处理: 平台符号桥。
    #define 中文名 英文符号 → 预处理层展开回 DLL 符号 (种子/自举同路径)。
    词法器反向条目 → zh 编译器编译用户程序时把中文名改回英文 (自举源走 #define, 不冲突)。"""
    # 桥表: 中文名 -> DLL 符号 (英文符号作为平台地址数据, 仅出现在这里)
    BRIDGE = [
        ('错误流', 'stderr'),
        ('输出流', 'stdout'),
        ('输入流', 'stdin'),
        ('取环境', 'getenv'),
        ('判十六进制', 'isxdigit'),
        ('格式化限长', 'snprintf'),
        ('取指数', 'exp'),
    ]
    # 1) 顶层 #define 桥 (插在首个块注释之后)
    bridge = ('\n/* 平台符号桥 (批3b/3c): 中文名 = DLL 符号 — 机器码层面的地址数据, 预处理层展开\n'
              '   (词法器反向条目供用户程序使用) */\n')
    for zhn, en in BRIDGE:
        bridge += '#define %s %s\n' % (zhn, en)
    first = out.find('*/')
    if first >= 0:
        out = out[:first + 2] + bridge + out[first + 2:]
    # 2) 词法器反向条目 (插在库函数表 isalnum 行后)
    lex_entries = '\n'
    for zhn, en in BRIDGE:
        lex_entries += ('            否 若 (!比较字符串(词名表[词序], "%s")) 复制字符串(词名表[词序], "%s");\n' % (zhn, en))
    anchor = '否 若 (!比较字符串(词名表[词序], "判字母数字")) 复制字符串(词名表[词序], "isalnum");'
    aidx = out.find(anchor)
    if aidx < 0:
        # 回退: 用英文原形找 (库函数表在批3a 前)
        anchor2 = 'if (!strcmp(tn[ti], "isalnum")) strcpy(tn[ti], "isalnum");'
        aidx = out.find(anchor2)
        if aidx >= 0:
            out = out[:aidx] + lex_entries.replace('比较字符串', 'strcmp').replace('复制字符串', 'strcpy') + out[aidx:]
    else:
        out = out[:aidx + len(anchor)] + lex_entries + out[aidx + len(anchor):]
    out = layer_change(out)
    out = pp_directives(out)
    return out


def pp_directives(out):
    """预处理指令词中文化 (双认: 英文指令保留 C 头兼容, 中文指令词第一等)。
    逐处把 !比前n(ARGS,"en",L) && (边界) 扩展为 (英文分支 || 中文分支)。
    边界偏移同步改 (中文指令词字节数不同)。"""
    import re as _re
    DIR = {
        'define': ('定义词', 9),
        'include': ('包含', 6),
        'ifdef': ('若有定义', 12),
        'ifndef': ('若无定义', 12),
        'endif': ('结束若', 9),
    }
    pat = _re.compile(r'!比前n\(([^,]+),\s*"(' + '|'.join(DIR) + r')",\s*(\d+)\)(\s*&&\s*\([^;]*?\))')
    def _sub(m):
        args, en, ln, bound = m.group(1), m.group(2), m.group(3), m.group(4)
        zhn, zlen = DIR[en]
        bound_zh = _re.sub(r'(\+ ?|\[)' + ln + r'\]', lambda m: m.group(1) + str(zlen) + ']', bound)
        en_branch = '!比前n(%s, "%s", %s)%s' % (args, en, ln, bound)
        zh_branch = '!比前n(%s, "%s", %s)%s' % (args, zhn, zlen, bound_zh)
        return '((' + en_branch + ') || (' + zh_branch + '))'
    n = len(pat.findall(out))
    out = pat.sub(_sub, out)
    # 读名字偏移修复: ifdef/ifndef 后续读名字用英文长度硬编码 (X + (是否条件编译 ? 5 : 6)),
    # 中文指令词 (若有定义/若无定义 12 字节) 会指到词中间 → 改为「跳指令词」, 中英通用。
    pat_r1 = _re.compile(r'整 ([^ =]+) = ([^ =]+) \+ \(是否条件编译 \? 5 : 6\);')
    def _fix_arr(m):
        var, base = m.group(1), m.group(2)
        return ('整 %s = %s; 循环 (判字母数字((无 字)串值[%s]) || 串值[%s] == \'_\' || ((无 字)串值[%s] >= 0x80)) %s++;'
                % (var, base, var, var, var, var))
    out, n1 = pat_r1.subn(_fix_arr, out)
    pat_r2 = _re.compile(r'常 字 \*([^ =]+) = ([^ =]+) \+ \(是否条件编译 \? 5 : 6\);')
    def _fix_ptr(m):
        var, base = m.group(1), m.group(2)
        return ('常 字 *%s = %s; 循环 (判字母数字(*%s) || *%s == \'_\' || ((无 字)*%s >= 0x80)) %s++;'
                % (var, base, var, var, var, var))
    out, n2 = pat_r2.subn(_fix_ptr, out)
    # 毛简 (pp_def_parse) 跳 define 长度: 寻 += 6; -> 寻 += (寻[0] == 'd' ? 6 : 9);
    old_mj = '寻 += 6; /* skip #  define'
    if old_mj in out:
        out = out.replace(old_mj, "寻 += (寻[0] == 'd' ? 6 : 9); /* skip #  define", 1)
    # 其余 define 读名字偏移 (+6 -> 6/9 条件化, 限定 pp 阶段变量名防误伤)
    # 副组 (段 数组): 循环 (段[...] 跟随
    pat_d = _re.compile(r'整 (寻三|指|寻) = (元|寻二|计|指|指深度) \+ 6; 循环 \(段\[')
    def _mk_d(m):
        return "整 %s = %s + (段[%s] == 'd' ? 6 : 9); 循环 (段[" % (m.group(1), m.group(2), m.group(2))
    out, nn = pat_d.subn(_mk_d, out)
    # 串值 数组版本
    pat_a = _re.compile(r'整 (寻三|指|寻) = (元|寻二|计|指|指深度) \+ 6;')
    def _mk_arr(m):
        return "整 %s = %s + (串值[%s] == 'd' ? 6 : 9);" % (m.group(1), m.group(2), m.group(2))
    out, nn = pat_a.subn(_mk_arr, out)
    pat_p = _re.compile(r'常 字 \*(寻) = (指|指深度) \+ 6;')
    def _mk_ptr(m):
        return "常 字 *%s = %s + (*%s == 'd' ? 6 : 9);" % (m.group(1), m.group(2), m.group(1))
    out, nn = pat_p.subn(_mk_ptr, out)
    pat_c = _re.compile(r'计 = 元 \+ 6;')
    out, nn = pat_c.subn("计 = 元 + (串值[元] == 'd' ? 6 : 9);", out)
    # 函数宏读名字循环补 0x80 (中文宏名)
    old_fn = "循环 (判字母数字((无 字)串值[指]) || 串值[指] == '_') { 若 (名序 < 63) 名存[名序++] = 串值[指]; 指++; }"
    if old_fn in out:
        out = out.replace(old_fn, "循环 (判字母数字((无 字)串值[指]) || 串值[指] == '_' || ((无 字)串值[指] >= 0x80)) { 若 (名序 < 63) 名存[名序++] = 串值[指]; 指++; }", 1)
    # 「预处理行不展开」守卫加中文 (定义词/若有定义 等首字 0x80)
    for arr in ['串值', '段']:
        old_pp = ("若 (%s[寻二] == 'd' || %s[寻二] == 'u' || %s[寻二] == 'i' || %s[寻二] == 'e' || "
                  "%s[寻二] == 'l' || %s[寻二] == 'p' || %s[寻二] == 'n')" % tuple([arr] * 7))
        if old_pp in out:
            out = out.replace(old_pp, old_pp[:-1] + ' || ((无 字)%s[寻二] >= 0x80))' % arr, 1)
    return out


def layer_change(out):
    """改层 (大哥三步: 中文 token 第一等):
    1) 词法器删「甲言关键字: 中文→英文名」strcpy 改写链 (中间商)
    2) kw() 直接认中文关键字 (整→VK, 若→IK ... 与英文平级)
    3) parser 的 strcmp(tn,..) 检查加中文等价 (静/常/无/长/字... 中文源 tn 不再被改写回英文)
    C 头英文解析保留 (kw 英文条目不动)。"""
    import re as _re
    # ---- 中文关键字 -> 英文 (c_to_jy EN2ZH + 甲言扩展别名) ----
    KW_MAP = {
        'struct': '构', 'union': '联', 'enum': '枚',
        'return': '返', 'if': '若', 'else': '否',
        'while': '循环', 'do': '做', 'for': '遍',
        'break': '断', 'continue': '续', 'switch': '择',
        'case': '例', 'default': '缺', 'goto': '跳',
        'int': '整', 'double': '双', 'float': '单',
        'char': '字', 'void': '空', 'short': '短',
        'long': '长', 'const': '常', 'static': '静',
        'unsigned': '无', 'sizeof': '大小',
        'volatile': '易失', 'defer': '延迟', 'select': '择收',
    }
    # 双字/别名: 英文 -> [中文...] (改写链里有的, 改层后 kw 直接认)
    KW_ALIAS = {
        'return': ['返回'], 'else': ['否则'], 'continue': ['继续'],
        'char': ['字节'], 'long': ['长整'],
    }
    # kw() 返回的中文 token 常量 (批1 映射名)
    KW_TOK = {
        'struct': '结构词', 'union': '结构词', 'enum': '枚词',
        'return': '后甲', 'if': '顺轮', 'else': '虚伍',
        'while': '下得一', 'do': '副丁', 'for': '净吉',
        'break': '标利', 'continue': '下退一', 'switch': '开关',
        'case': '案例', 'default': '缺省', 'goto': '大于',
        'int': '型词', 'double': '型词', 'float': '型词',
        'char': '型词', 'void': '型词', 'short': '型词',
        'long': '型词', 'const': '型词', 'static': '型词',
        'unsigned': '型词', 'volatile': '型词',
        'sizeof': '度序',
    }
    # ---- 3) parser strcmp(tn,..) 检查加中文等价 (先做, 避免误伤改写链删除后的文本) ----
    # 带 ! 的: !比较字符串(X,"long") → (!比较字符串(X,"long") || !比较字符串(X,"长"))
    #   (语义: long 或 长 命中 — 每分支各带 !, 否则 !(A||B) 恒假)
    # 不带 ! 的: 比较字符串(X,"long") → (比较字符串(X,"long") || 比较字符串(X,"长"))
    for en, zhn in KW_MAP.items():
        alts = [zhn] + KW_ALIAS.get(en, [])
        # 先处理带 ! 的 (更长模式优先)
        pat_n = _re.compile(r'!比较字符串\(词名表\[([^\]]+)\],\s*"' + _re.escape(en) + r'"\)')
        def _sub_n(m, en=en, alts=alts):
            body = '!比较字符串(词名表[%s], "%s")' % (m.group(1), en)
            for a in alts:
                body += ' || !比较字符串(词名表[%s], "%s")' % (m.group(1), a)
            return '(' + body + ')'
        out, n = pat_n.subn(_sub_n, out)
        # 再处理不带 ! 的 (剩余; 负向断言排除第一正则刚生成的带 ! 调用, 防嵌套膨胀)
        pat = _re.compile(r'(?<!\!)比较字符串\(词名表\[([^\]]+)\],\s*"' + _re.escape(en) + r'"\)')
        def _sub(m, en=en, alts=alts):
            body = '比较字符串(词名表[%s], "%s")' % (m.group(1), en)
            for a in alts:
                body += ' || 比较字符串(词名表[%s], "%s")' % (m.group(1), a)
            return '(' + body + ')'
        out, n = pat.subn(_sub, out)
    # ---- 2) kw() 加中文条目 (插在「此处只认英文」注释后) ----
    kw_anchor = '/* 甲言中文关键字已在 lexer 改写成英文 (tn), 此处只认英文 */'
    kw_lines = '\n    /* 改层 (2026-08-31): 中文关键字第一等 — 直接认中文, 不经过英文改写 */\n'
    for en in sorted(KW_MAP):
        tok = KW_TOK.get(en)
        if not tok:
            continue
        zhn = KW_MAP[en]
        alts = [zhn] + KW_ALIAS.get(en, [])
        conds = ['!比较字符串(串值, "%s")' % a for a in alts]
        kw_lines += '    若 (%s) 返 %s; /* %s */\n' % (' || '.join(conds), tok, en)
    kidx = out.find(kw_anchor)
    if kidx >= 0:
        out = out[:kidx + len(kw_anchor)] + kw_lines + out[kidx + len(kw_anchor):]
    # ---- 1) 删词法器关键字改写链 (从「甲言关键字」注释到 易失 行) ----
    rm_start = out.find('/* 甲言关键字: 中文 → 英文名')
    if rm_start >= 0:
        rm_end = out.find('"易失")) 复制字符串(词名表[词序], "volatile");', rm_start)
        if rm_end >= 0:
            rm_end += len('"易失")) 复制字符串(词名表[词序], "volatile");')
            out = out[:rm_start] + out[rm_end:]
    return out


def main():
    map_path = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / 'scripts' / 'zh_map.txt')
    mapping = load_map(map_path)
    src = ROOT / 'srclib_jiayan' / 'qcc_work.jy'
    dst = ROOT / 'srclib_jiayan' / 'qcc_zh.jy'
    text = src.read_bytes().decode('utf-8')
    out = transform(text, mapping)
    out = post_process(out)
    dst.write_bytes(out.encode('utf-8'))
    print(f'applied {len(mapping)} mappings + 批3b 平台流桥 -> {dst} ({len(out)} chars)')


if __name__ == '__main__':
    main()
