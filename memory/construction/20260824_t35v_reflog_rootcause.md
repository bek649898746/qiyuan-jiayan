# t35v 施工日志 — reflog 崩溃根治（sizeof 双层解引用 + 逗号续行 struct 字段 + fseek no-op + fread 返回值）

> 日期: 2026-08-24 | 提交: t35v | 新不动点: 3b4002d2（v1==v5 1-cycle 闭包, H1==H2 双链合一）
> 验证: build.ps1 全绿 325/325 + behavior 20/20 + git 综合回归 24/24 + reflog 全链路对照原生一致

## 症状
`git reflog` / `git log -g` / `rev-parse HEAD@{N}` 全线崩溃或空输出：
- 初版: add_reflog_for_walk 写 info->logs 时 NULL 解引用 SEGV
- 修复 sizeof 后: reflog walk 通但 rev-parse HEAD@{1} 报 "log for HEAD is empty"

## 根因链（四个独立 qcc bug，层层剥开）

### Bug A: qcc `sizeof(**pp)` / `sizeof(*(*pp))` 双层解引用只解析一层 → 8
- `init_reflog_walk` 里 `CALLOC_ARRAY(*info, 1)` → `sizeof(*(*info))` = `sizeof(struct reflog_walk_info)` 应为 72
- qcc 只解一层 → 8 → `xcalloc(1, 8)` 只分配 8 字节
- info->nr/alloc/complete_reflogs 全部越界写堆垃圾 → ALLOC_GROW 外层判断 `(nr+1)>alloc` 遇垃圾 alloc 恒假 → 永不 GROW → logs=NULL → SEGV
- **修复**: sizeof 解析的 DK 分支（3708）+ 括号链分支（3729）支持连续 `*`，deref_n>=2 时用 `var_pinner`（最深层基类型大小）

### Bug B: qcc 逗号续行 struct 字段大小不继承 → 4 字节
- `struct reflog_info { struct object_id ooid, noid; ... }` 的 `noid`（逗号续行）fsz 落默认 4
- reflog_info 布局 36+4 vs 36+36 → items 数组步长错 → next_reflog_commit 读 entry->noid 错位 → parse_object 全 NULL → log -g 无输出
- **修复**: 三处逗号续行继承（4700 本地路径 sty_persist / 5824 v2t_si_persist / 5907 if_sty_persist），续行字段继承 struct 大小

### Bug C: fseek 分派实现是 no-op → SetFilePointer 不调用
- `prim()` builtin 分派里 fseek 直接 `mov_r_imm(0,0)`（no-op）
- files_for_each_reflog_ent_reverse 用 fseek/ftell 反向读 reflog → fseek 不生效 → 位置不动 → 读空 → read_ref_at reccnt=0 → "log is empty"
- **修复**: fseek/rewind 仿 ftell 实现 SetFilePointer(handle, offset, NULL, origin)；origin 需在 r8 被 high=NULL 覆盖前先存 r9

### Bug D: fread 返回字节数而非元素数
- `fread(buf, cnt, 1, logfp)` 应返回 1（元素数），qcc 返回字节数 403
- files-backend 检查 `nread != 1` → "cannot read 403 bytes"
- **修复**: fread 返回值 = bytes/size（保存 size 到 r14，ReadFile 后 DIV）

## 修复明细（srclib/qcc_x86.c, 均加 t35v 注释）
1. sizeof DK 分支: deref_n 多层解引用（*p=1, **p=2）
2. sizeof 括号链: deref2 括号内再解引用
3. 本地 struct 逗号续行: sty_persist>=0 继承 fsz
4. u2 嵌套 v2 循环: v2t_si_persist 持久化
5. inline body if 循环: if_sty_persist 持久化
6. fseek: SetFilePointer(handle, offset, NULL, origin)，origin 先存 r9
7. rewind: SetFilePointer(handle, 0, NULL, FILE_BEGIN)
8. fread: 返回 bytes/size（r14 存 size, DIV r9d）

## 验证
- 自举: qcc_boot.exe(新种子) → v1→v5 全等 3b4002d2（1-cycle 不动点）
- H1==H2: qcc_x86.exe(C源) vs v1.exe(镜像) 编译 _ftell3.c/reflog-walk.c 输出哈希一致
- git 回归: 24/24 PASS
- reflog 全链路: reflog / log -g / rev-parse HEAD@{0..2} / reflog show / reflog --all 对照原生一致
- 单测: fseek(100,SET) rc=100 ftell=100; fread n=1（元素数）
- 其他: status/log/count-objects 正常

## 待办
- run_tests.py 325/325 最终确认（bin_test 裸机死循环测试用 taskkill /T 修复 kill 残留）
- 记忆库写入
