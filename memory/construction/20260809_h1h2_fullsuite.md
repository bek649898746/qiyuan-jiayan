# 2026-08-08/09 H1==H2 全量攻坚 — 164/167 → 167/167 + 新不动点 EDD810EB

## 一句话
全量 H1==H2（167 测试 host vs v2 产物 SHA256）从没人真正跑过 → 首跑 164/167 → 三处 H1!=H2 分叉全部根治 → **167/167 全绿**。新自举不动点 **EDD810EB**（v2==v3==v4）。新增回归测试 regress_pptr_postinc.c（168/168）。

## 三个 H1!=H2 分叉（全记录）
| 测试 | 症状 | 根因 | 修复 | 解冻批 |
|---|---|---|---|---|
| bin_test.c | v2 挂死 (0xC0000005 编译期) | 镜像缺 `__attribute__` 词法吞噬 (宿主 L2504) | 镜像 lexer 加吞噬 (安全区) | 第3批 → 54A07F01 |
| regress_fn_macro_nest.c | v2 产物 235B 空壳 (main 整个消失) | **prim() 括号后缀链缺 PP/MM 分支 → `(*p)++` 被吞** → fn_macro `out[(*o)++]` 索引不推进 | 宿主+镜像: 括号/强转后缀链补 PP/MM + case-12 加 cg_no_deref 分支 | 第4批 → D524D3E1 |
| regress_deep_static_bigparam.c | 产物差 5 字节 | 镜像缺 08-03 宿主 lea disp8 修复 (`lea rax,[rsp+0]`) | 镜像 L5790 对齐 `modrm(1,0,4);b(0x24);b(0)` | 第5批 → EDD810EB |

## `(*p)++` 根因详解（本会话最大发现）
- **是 PARSER bug 不是 codegen**：prim() 三个后缀链循环（变量/强转/括号），只有变量链有 PP/MM 分支
- 宿主也中招（宿主产物 `(*o)++` 也输出 0）—— 宿主靠 gcc 编译才自洽，v2 靠自己坏的 `(*o)++` 跑 fn_macro → 全毁
- 独立复刻 fn_macro 全套（括号平衡提取宿主函数）→ gcc 编 vs v2 编对比 → 崩在纯复制 → 探针四变体秒分
- 镜像补丁教训：甲言 `否 断;`(else break) 必须放在新分支**之后**，放前面=不可达（第一版补丁此错）

## 验收数据
- 全量 H1==H2: **167/167 通过**（`python scratch_test\_h1h2_all.py`，host vs v2 产物 SHA256，超时计失败）
- 新回归: regress_pptr_postinc.c H1==H2 PASS + 双产物运行 `pptr-postinc-ok rc=0`
- 不动点: v1=623498B4(宿主系, 不需相等) / v2==v3==v4=**EDD810EB**
- 宿主: qcc_x86.c 8033 行 551 fix / 镜像: 8059 行 452 fix

## 工具教训
- **run_tests.py 启动清空 scratch_test**（CI 修复 L26-32）→ 探针脚本别放那或及时备份
- bin_test run 崩溃是既有行为（裸机内核桩写 0xB8000 不能跑 Windows），非回归（新旧宿主产物逐字节一致实证）
- build.ps1 旧校验（v1==v2==v3==A9CC10A2）过时 → 已更新为 v2==v3==v4==EDD810EB
- 验收标准确认: **v2==v3（自举闭环）**，v1≠v2 正常（gcc 系 vs 甲言系）

## 提交记录
- qcc_x86.c: parser 后缀链 PP/MM ×2 + case-12 cg_no_deref（3 处）
- qcc_work.jy: attribute 吞噬 + parser 后缀链 ×2 + case-12 cg_no_deref + lea disp8（5 处）
- scripts/build.ps1: 不动点校验更新
- README/docs/方案v1.1: 数据刷新
- tests/qcc/regress_pptr_postinc.c: 新回归
