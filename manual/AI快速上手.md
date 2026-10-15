# 甲言 · AI 快速上手

**面向**：想让 AI（LLM / Agent）理解、分析、修改甲言代码的人。
**目标**：让 AI **不必逐个读源文件**，而是通过甲言在**编译期导出的结构化数据**直接掌握代码。

> 本页所有命令与输出 **2026-10-15 本机实测**，含实测原文。

---

## 一、为什么甲言对 AI 友好

传统做法是让 AI 把成百上千个源文件塞进上下文 —— 慢、贵、且容易漏。

甲言把两样**机器可读**的东西做在了编译期：

| 接口 | 产物 | AI 能直接拿到什么 |
|:--|:--|:--|
| **代码知识图谱** | `*.json` | 函数 / 类型 / 调用关系（谁调谁 · 扇入扇出 · 孤立函数 · 结构体布局风险） |
| **结构化诊断** | JSON（stderr） | 错误码 · 位置 · 消息 · **修复建议** · **出错源码行** · 因果链 · 回溯 |

⟹ AI 可以**查询**结构，而不是**阅读**全文。

---

## 二、接口一：代码知识图谱（推荐首选）

### 第 1 步 · 导出图谱

```powershell
.\qcc_x86.exe -graph 图谱.json 你的源文件.jy
```

> ### 🔴 参数顺序坑（**实测踩过，务必注意**）
> `-graph` 后面紧跟的是 **输出文件名**，源文件要放在**更后面**。
>
> **如果写反**（例如 `qcc_x86.exe -graph 源码.jy`），编译器会把那个路径当作**输出目标**，
> **直接覆盖你的源码文件** —— 实测 `examples/成绩.jy` 从 1601 字节被写成 17KB 的 JSON。
>
> ✅ 正确：`-graph <输出.json> <源文件>`
> ❌ 危险：`-graph <源文件>`

实测输出：

```text
OK: a.exe (270011 bytes code)
[graph] 图谱.json 导出完成 (fn:207 st:3 calls:66)
```

### 第 2 步 · 查询（8 种）

```powershell
python scripts/graph_query.py 图谱.json callers 函数名    # 谁调用它
python scripts/graph_query.py 图谱.json callees 函数名    # 它调用了谁
python scripts/graph_query.py 图谱.json chain   函数名 5  # 调用链（向上 N 层，默认 3）
python scripts/graph_query.py 图谱.json risk              # 结构体布局风险
python scripts/graph_query.py 图谱.json fanout            # 扇出最大（调用了最多函数）
python scripts/graph_query.py 图谱.json fanin             # 扇入最大（被最多函数调用）
python scripts/graph_query.py 图谱.json funcs             # 列出所有用户函数
python scripts/graph_query.py 图谱.json isolated          # 孤立函数（无调用边）
```

实测输出：

```text
$ python scripts/graph_query.py 图谱.json funcs
user functions (75):
  fn:CloseHandle
  fn:ConvertThreadToFiber
  fn:CreateFiber
  fn:CreateFileA
  ...

$ python scripts/graph_query.py 图谱.json fanout
top fanout:
  fn:__ws_worker_loop: 4
  fn:__qcc_ws_submit_child: 4
  fn:__qcc_ws_children_join: 3
```

> **给 AI 的调用建议**：先用 `funcs` 拿函数清单 → 对关键函数用 `callers`/`callees` 判影响面
> → 需要溯源时用 `chain`。比全文检索省一个数量级以上的 token。

---

## 三、接口二：结构化诊断

```powershell
.\qcc_x86.exe --diagnostics=json 你的源文件.c     # 简写：--dj
```

正常编译无诊断时**不输出**（无错即无 JSON）；**有诊断时**输出到 stderr。

**实测输出**（源文件故意写 `int 甲; printf("%d", 甲);`）：

```json
{"diagnostics":[{
  "code":"A4001",
  "severity":"error",
  "loc":{"file":"D:\\...\\_d4.c","line":3,"col":0},
  "message":"明确初始化: 变量 '甲' 未初始化就读取 (声明须初始化或先赋值, v3.0 #78)",
  "fixes":["声明时初始化, 或先赋值再读取 (明确初始化 #78)"],
  "codeLine":"    printf(\"%d\", 甲);",
  "causeChain":[],
  "backtrace":[]
}]}
```

**字段说明**：

| 字段 | 含义 |
|:--|:--|
| `code` | 错误码（如 `A4001`），详见 `error_codes/<码>.md` |
| `loc` | 文件 / 行 / 列 |
| `message` | 中文错误说明 |
| **`fixes`** | **修复建议**（按错误码精确给出，不是占位文案） |
| **`codeLine`** | **出错那一行的源码**（AI 可直接定位改哪行） |
| `causeChain` / `backtrace` | 因果链与回溯（已预埋，逐步填充） |

### ⚠️ 接入范围（实测结论）

走**统一错误出口 `emit_error()`** 的错误会输出 JSON；
**部分检查点尚未接进该出口** —— 例如括号配对检查仍只输出纯文本：

```text
[ERR] 括号不匹配 '(' vs '}' (开 行 1492, 闭 行 1494) (paren_balance)
```

**AI 端判据**（用于容错）：
- 输出以 `{"diagnostics"` 开头 → 已接入，按 JSON 解析 ✅
- 输出以 `[ERR]` 开头 → 未接入，按正则提取错误码/行号 ⚠️

---

## 四、AI 用甲言的推荐工作流

```text
① 拿函数清单   python scripts/graph_query.py 图谱.json funcs
② 判影响面     ... callers <fn> / callees <fn>
③ 看结构风险   ... risk · fanout · fanin · isolated
④ 编译取诊断   qcc_x86.exe --diagnostics=json <src>   → 直接读 fixes / codeLine
⑤ 改完自检     powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1
```

第 ⑤ 步是**硬判据**：**自举链多代哈希全等** = 修改正确。
（注意：脚本里的 `$expected` 会随源码合法变动，**不要因为对不上 expected 就以为失败**。）

---

## 五、给 coding agent 的项目约定

若你是一个 AI coding agent，准备修改本仓库，**先读** [`AGENTS.md`](../AGENTS.md)：
里面写了编码铁律（`.ps1` 必须 UTF-8 with BOM）、自举判据、目录职责，以及提交前必须跑的校验入口。

---

## 六、其他为 AI 设计的能力（本页未展开）

README「核心能力」表还列有：**编译 DAG 重放证明** · **编译检查点**。
这些的使用方式与上面两个接口不同，建议先用 `.\qcc_x86.exe`（无参数）看 Usage，
或直接查阅源码中的参数解析段（`srclib/qcc_x86.c` 的 main 参数循环）。

---

_本页：启元 · 2026-10-15 · 全部命令均在本机实测，含实测输出原文；发现的 `-graph` 覆盖源文件问题已同步递刀。_
