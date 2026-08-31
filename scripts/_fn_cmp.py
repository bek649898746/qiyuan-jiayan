# -*- coding: utf-8 -*-
import io, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
V2 = ROOT + r'\qcc_zh_v1.exe'

def test(name, src):
    p = ROOT + r'\scratch_test\fc_' + name + '.jy'
    io.open(p, 'w', encoding='utf-8').write(src)
    r = subprocess.run([V2, p, '-o', p + '.exe'], capture_output=True, text=True)
    print('%s: rc=%d %s' % (name, r.returncode, (r.stdout + r.stderr)[:130]))
    if r.returncode == 0:
        r2 = subprocess.run([p + '.exe'], capture_output=True, text=True)
        print('  run: %s rc=%d' % (r2.stdout.strip()[:60], r2.returncode))

# 英文函数宏
test('en_fn', '#define SQUARE(x) ((x)*(x))\nint main() { printf("%d\\n", SQUARE(7)); return 0; }\n')
# 英文函数宏 + 中文关键字
test('en_fn_zh', '#define SQUARE(x) ((x)*(x))\n整 main() { 格式化打印("%d\\n", SQUARE(7)); 返 0; }\n')
# 中文函数宏名 + 英文宏
test('zh_fnname', '#定义词 SQUARE(x) ((x)*(x))\n整 main() { 格式化打印("%d\\n", SQUARE(7)); 返 0; }\n')
