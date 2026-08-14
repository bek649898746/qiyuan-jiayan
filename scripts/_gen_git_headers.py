# -*- coding: utf-8 -*-
"""生成 Git 的 make 生成头文件 (command-list.h / hook-list.h / config-list.h) 占位."""
import io, os

GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'

# command-list.h: struct cmdname_help + command_list[] (最小占位, help.c 只要求非空)
cmdlist = '''/* Automatically generated (placeholder) */
struct cmdname_help {
    const char *name;
    const char *help;
    uint32_t category;
};
static const char *category_names[] = {
    NULL
};
static struct cmdname_help command_list[] = {
    { "add", "Add file contents to the index", 1 },
};
'''

hooklist = '''/* Automatically generated (placeholder) */
static const char *hook_name_list[] = {
    "applypatch-msg",
    NULL,
};
'''

configlist = '''/* Automatically generated (placeholder) */
static const char *config_name_list[] = {
    "core.filemode",
    NULL,
};
'''

for name, content in [('command-list.h', cmdlist), ('hook-list.h', hooklist), ('config-list.h', configlist)]:
    p = os.path.join(GIT, name)
    if not os.path.exists(p):
        io.open(p, 'w', encoding='utf-8', newline='\n').write(content)
        print('生成', name)
    else:
        print('已存在', name)
