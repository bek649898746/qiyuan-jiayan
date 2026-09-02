# build_zh.ps1 — 中文版甲言 (qcc_zh.jy) 自举验证
#
# 流程:
#   1. python scripts/sinicize.py   # 按 scripts/zh_map.txt 把 qcc_work.jy 生成 qcc_zh.jy
#   2. qcc_x86.exe (英文种子) 编译 qcc_zh.jy -> qcc_zh_v1.exe
#   3. qcc_zh_v1.exe 自编译 qcc_zh.jy -> v2 -> v3 -> v4 -> v5
#   4. v1..v5 必须收敛到 $expected (中文不动点)
#
# 英文链不动点 (build.ps1) 不受影响: qcc_x86.c 从未修改。

param([switch]$Regenerate)
$ErrorActionPreference = 'Stop'
$expected = '34A38117'  # 2026-09-02 #79 C++互操作 真接入: extern "C++"/extern "C" 链接规格串接受 (L14164 extern 分支吞串) + 类 关键字 = 构 别名 (词法器 L4237 结构词); 原 6D18C399  # 2026-09-02 #74 类型级编程 真接入: 判类型(x, T) 编译期类型谓词 — 变量类型名 == T → 常量 1/0 (原级 表达式层 + 末列 常量层 双分支 + L7342 排除表), 配合 编译期断言/编译期 类型级计算; 原 E699A57A  # 2026-09-01 #25 结构化并发 真接入: 结构化并发 { 体 } — 块内 线程 自动登记 (结构化线程表 8层×16) + 块退出自动 join 全部子线程 (thread_join(&t) 循环); 嵌套分层 + 块外不登记; L7342 排除表加 结构化并发; 原 02F22405  # 2026-09-01 #86 内存泄漏检测 真接入: 泄漏报告; → __qcc_leak_report(); — qcc_rt.c 报告运行时资源泄漏 (arena 池未释放 [LEAK] 报告); qcc 内建 malloc 是提升分配器无泄漏概念 (边界); L7342 排除表加 泄漏报告; 原 E769DD24  # 2026-09-01 #93 内联 真接入 (inline 展开): 净集 预扫描 inline 函数 (英文) → 注册函数宏 (体=单返表达式), 调用点副组 展开 (复用泛型机制); 中文 内联 L15142 顶层跳过落 C 函数定义路径 (不映射型词); 坑: 词法器 内联→型词 破坏 类型表[].内联 字段名 → 自举错乱卡死 → 回退 + 顶层跳过; 原 77048F2A  # 2026-09-01 #82 Arena 分配器 内置运行时 真接入: qcc_rt.c 加 __arena_create/alloc/reset/free (大块池 malloc + 偏移分配, 重置回零复用, 懒建池默认 64KB) + 区域 映射升级 (分配→__arena_alloc, 重置→__arena_reset, 释放→__arena_free, 新增 建→__arena_create); 坑: 替换残留 } 括号 (懒建池合并); 原 EEDEEDC7  # 2026-09-01 #36 显式别名管理 restrict 真接入: 修 块声明路径 (L7470/7473) + 参数路径 (L15177) 指针限定符跳过 restrict (原 restrict 当变量名解析崩) + A5002 别名冲突检测 (restrict 指针初始化 &基对象, 同基被多 restrict 占用 → A5002); 限止表 回溯扫描; 原 2E0A613F  # 2026-09-01 #20 异步/await 语言级 真接入: 等候 表达式; → __gen_yield_v (fiber 挂起传值给事件循环, await 语义, 与 产出 同机制); 异步任务 = 生成(g,异步函数,ctx) 创建 + 下一个(g,v) 步进 (fiber 状态机语言级); 坑: 等待 已被线程 join 占用 (等待 t → thread_join) → 用 等候; 原 E32C338B  # 2026-09-01 #19 actor 语言级邮箱 真接入: actor 名, 容量; 声明邮箱 (环形缓冲 __ac_名_buf/h/t/c/cap, 仿 通道) + 投递 名,v; → 内联入队 (若 c<cap) + 接收 名,v; → 内联出队 (若 c>0 FIFO) — 未声明回退 actor_send/recv (兼容旧); L7342 排除表加 actor; 原 D325E990  # 2026-09-01 #55 异常完整 真接入 (真异常栈): 试 { } 捕获 { } 块级 try/catch — 抛出 在试块内 → throw_ex + 跳转捕获标签 (同函数异常栈, 跳过中间语句); 试体末尾 若(ex_pending==0) 跳结束 (跨函数传播); 捕获后 ex_pending=0; 坑: qcc 中文变量名高频单字 (数/跳/零/比较/值子) 嵌套块查找遮蔽缺陷 → 试 分支变量全用 试* 独特前缀; 原 51AF5480  # 2026-09-01 #24 select 多路复用 真接入: 就绪(q)→__ch_q_c>0 (收ready) + 就绪送(q)→__ch_q_c<__ch_q_cap (送ready) 编译器级通道状态检测原语, 与 择收 条件链组合 = Go select 非阻塞多通道轮询; L7342 排除表加 就绪/就绪送; 原 AE59C852  # 2026-09-01 #56 生成器完整 真接入 (语言级 yield): 生成(g,fn,ctx)→__gen_init (fiber 实例) + 下一个(g,x)→__gen_next (步进 1有值 0完成) + 产出 x;→__gen_yield_v(x) (yield 挂起传值); 复用 stackful fiber (qcc_rt.c 增 __gen_val/__gen_act/三函数); 坑: 被调者最后 参数先接; 原 C619E56E  # 2026-09-01 #68 模板元编程 CTFE 递归 真接入: 编译期 f(常量...) 函数间递归/嵌套调用 (阶乘/斐波那契/平方和 纯函数) — 末列 词变量分支加函数调用 (词变量+左括 → 中属) + 中属/毛重 参数绑定 推帧隔离 (宽劣=基 隐藏外层同名 + 压帧 强制压栈 不查同名, 内层参数永不覆盖外层帧); 原 2C307577  # 2026-09-01 泛型回归完全修复 (偏移修正): "泛型" 2 汉字 6 字节 (泛E6B39B+型E59E8B) — 原 `?6:9` UB 且 9 错; 净集 L894/副组 L657 改 +6 → 泛型注册+展开全恢复 (feat_generic_zh PASS); 原 F553B2E7  # 2026-09-01 泛型 UB 崩溃修复 (真回归排查): 净集 L894 `指+(*寻=='d'?6:9)` 定义前解引用 UB → 指+9; 副组 L657 同 UB → 计+9; 净集 改扫 串值 (原 后弊 经 行申/原从 处理后泛型行丢失); 泛型从 0xC0000005 崩溃 → 正常报错; 展开失效待办; 原 EE69BE6A  # 2026-09-01 阶段C-1 顶层全局 typeof/_BitInt 缺口修复 (大哥验收): 顶层全局声明改写起始槽为对应型词 (typeof→整/双/字/短/长; _BitInt→有符号+类型, L14305 列利认) + 暂粗=1 (L14300 普通全局注册需); 原 9E9BBA27  # 2026-09-01 阶段C-1 #198 虚函数覆盖 override: 接口继承里 覆盖 方法名; 显式覆盖标记 + 父接口方法检查 A9006 (虚标修复, 原0实现); 原 337F675B  # 2026-09-01 阶段C-1 C23 _BitInt(N) 位宽整数: 源辰触发 L7000 通用注册 (数组/指针全形态) + L7106 (N) 识别 (无前缀+unsigned 前缀) + sizeof 位宽对齐字节 (双括号消费修复); C23 地基收官; 原 E6FA56F2  # 2026-09-01 阶段C-1 C23 #elifdef/#elifndef 预处理简写: 等价 #elif defined(名)/#elif !defined(名) (词法器指令识别 L519/L4111 下环二/下环三 + 读宏名 + 条件=定义/!定义 + 处理分支); 原 13C894CA  # 2026-09-01 阶段C-1 C23 char8_t UTF-8 字符: 变量声明兜底 (uint8_t 同语义 1字节无符号) + sizeof 三处 (L5446 变量分支兜底/L5563 标量分支/L5574 型定<0 兜底) + sizeof 变量分支 标准类型名兜底; 原 D63EBDA3  # 2026-09-01 阶段C-1 C23 枚举底层类型: 枚 名 : 类型 { } 枚底表 (枚底名+字符/短/长/无符号) + 顶层 L13348/块内 L7016 冒号解析记录 + 枚举变量按底层宽度注册 + sizeof 枚底查询 (L5500); 原 8FA9A458  # 2026-09-01 阶段C-1 C23 typeof 类型查询: typeof(x) y = init; 复制变量/类型 类型标志 (变量查询函数+类型关键字兜底; 类型取 双认); 原 959068FF  # 2026-09-01 阶段C-1 C23 nullptr 空指针字面量: nullptr/空指针 双认 (原级 与 NULL 同值0节点, 赋任意指针+与指针比较+条件); 原 73282B12  # 2026-09-01 阶段C-1 #193 单继承: 构 子 : 父 { } 基类字段置首复制 (正聚原样复制零漂移) + 调用 分派父结构体链回退 (方法继承) + 负例 A9004父未声明/A9005联合不可继承; 原 29A088DC  # 2026-09-01 阶段C-1 #197 封装/访问控制完善: 私有/公开 前置+后置标记全修 (①函数定义 L14429 保留私有标记 ②变量逆酉设/清而非是否静态标 ③全局注册5处查私有集合); 原 2D6CC486  # 2026-09-01 阶段C-1 #36+#37 生命周期: 借用 存期 _ x; 匿名域 (通配不查域) + 存期 a: b; 域约束 (父域必须已声明 A3005); 原 5A111A8A  # 2026-09-01 阶段C-1 #68 背压: 背压 送 q, v; — 通道满时发送阻塞 (Go 有界通道语义, 自旋等待 循环(__ch_q_c>=__ch_q_cap){} + 内联入队); 原 6EDB76D2  # 2026-09-01 阶段C-1 #115 trait 继承: 接口 子 : 父 { 方法 } 合并父方法 + 同名覆盖 + 调用分发回溯父接口实现绑定; A9004 父接口未声明; 坑: 语句() 有两处接口分支 (L8500 真/L8548 死代码), 修改必须落在 L8500; 原 643A912C  # 2026-09-01 阶段C-1 #116 impl Trait (二步 返回检查): return 变量类型必须实现 接口 (遍历 实现 表 宽失/旁从); A9005 拦截; 坑: 查函数有注册副作用→改遍历函数表; 原 3B19FCEE  # 2026-09-01 阶段C-1 #116 impl Trait (第一步 落IR): 函数 名() -> 实现 接口名 返回匿名实现类型 (箭头 token 语法); 函数表加 返接口 字段; vt_tyvar/空对 写接口索引+1; A9004 未声明接口拦截; 原 1F03F219  # 2026-09-01 阶段C-1 #4 生命周期标注 (二步 存期 域): 存期 a; 声明生命周期域 + 借用 存期 a x; 域绑定 → vt_lifetime_id/源粗 写域编号 (1..N); A3005 未声明域拦截; 块() 未知类型排除表加 存期 (原被吞当类型声明); 原 19ACA59B  # 2026-09-01 阶段C-1 #4 生命周期标注 (第一步 落IR): 借用状态机写入 vt_lifetime/总子 (2=不可变借用 3=可变借用 0=放还) + vt_lifetime_id/源粗 域编号 + 借用表压缩同步; 行为不变 (借用族 5 测试过); 原 84D61F19  # 2026-09-01 阶段C-0 IR 激活 (宏门 前亥一=1): 预埋结构位真实落位 (BSS 分离加载, 产物文件不变, 编译逻辑不变); 原 E96C7718  # 2026-09-01 阶段B 报错消息中文化收尾 (15 处用户可见英文→中文: csym/crel/paren_balance×4/at label/OOM×2/cannot×3/parse failed/annotation/I-O error; 保留 [ERR] 标签/A码/关键字/编译器名 qcc_x86) 后新不动点; 原 46dead87  # 2026-09-01 #231 repr 控制: 打包 构 { } 紧凑布局 (字段无对齐填充, 尺寸不轮 up); 原 2029a6ce  # 2026-09-01 #218 Maybe/Result 链: ?. 结构体值左操作数走普通 . 访问 (遍历变量表判断指深 — 查变量返回栈偏移非表索引); 原 6776247c  # 2026-09-01 #173 注解/属性完整: 注解 名=值; 顶层元数据标注 + --annotations 导出 (查注解 编译期查询); 原 96cd20e5  # 2026-09-01 #66 定时器/超时: 延时 N→timer_delay(N) / 超时 N→timer_deadline(N) 语法糖脱糖; 原 2b2e605e  # 2026-09-01 #181 宏卫生深化: #定义词 宏注册读名偏移条件化 (中文 9 字节, 原硬编码 +6 致中文宏不展开) + #未定义 双认; 原 1f5e68fb  # 2026-09-01 #289 DWARF 调试信息 (-g): PE 5 节 + .debug_abbrev/.debug_info/.debug_line 长节名 + 符号表/字符串表; 隔离实证: DWARF前后编译器无-g 产物 .text 逐字节一致; 原 e8258f76  # 2026-09-01 #279 名称空间 memmove 修复后新不动点 (删词元 复制字符串→移动内存 防重叠); 原 3819d542  # 2026-09-01 #279 名称空间/模块完整: 词元级 pass (块内符号加前缀 名_, 名::符号 合并) (Vec 迭代): 遍 x 于 v 支持 动态数组 — 递增节点修复 (加节点漏 __vi 操作数 → __vi 恒=2 死循环; 改 建节(23) 增量节点) + 复制长度 64→32  # 2026-08-31 #255 顶层重载修复 (主循环 L12325 加 重载 分支; 原顶层重载被变量声明吞 → main 丢失)  # 2026-08-31 中文化去吞 #105 枚举带负载  # 2026-08-31 中文化去吞 #105 枚举带负载 (变体(类型) 识别: 注册变体值+跳过类型参数, 不泄漏为成员)  # 2026-08-31 中文化去吞 #23 作用域守卫基础版  # 2026-08-31 中文化去吞 #23 作用域守卫基础版 (扫尾 = 函数退出执行, defer 路径; 块级待深化)  # 2026-08-31 中文化去吞 #136 通配符模式 _  # 2026-08-31 中文化去吞 #136 通配符模式 _ (池平=2 恒真比较, 匹配任何值不绑定)  # 2026-08-31 中文化去吞 #163 finally  # 2026-08-31 中文化去吞 #163 finally (无论如何 = 函数退出无条件执行, defer 别名; 中文主源模式首个特性)  # 2026-08-31 深度中文化后新不动点 (string_sinicize: 错误消息正文中文化 OOM→内存不足/AST→抽象语法树; 保留 [ERR] 前缀/纯英文判断词/asm/符号模板; 原 46ef0edb 是注释中文化版)  # 2026-08-31 深度中文化后新不动点 (string_sinicize: 错误消息正文中文化 OOM→内存不足/AST→抽象语法树; 保留 [ERR] 前缀/纯英文判断词/asm/符号模板; 原 46ef0edb 是注释中文化版)  # 2026-08-31 补 signed 中文词 (有符号) 后新不动点 (signed char 中文可写: 有符号 字; kw/strcmp/cast 全链认)  # 2026-08-31 中文链 cast 类型解析中文等价注入修复  # 2026-08-31 中文链 cast 类型解析中文等价注入修复 (layer_change 变量形式词名 strcmp: 转型型/转型型二/中粗) + MAX_LABELS 262144 扩容后新不动点; 原 4a19764d 缺 cast 中文等价 → (无 长 长) cast 8B 标记丢失 → 64 位移位/比较全崩 (t1 c64=0 复现)  # 2026-08-31 无整中文语法缺口修复后新不动点 (sinicize KW_ALIAS unsigned→无整; 原 19277eb6 缺无整连写)  # 2026-08-31 镜像同步修复后新不动点 (qcc_zh.jy 重生成含 ll_ext32/p_esz/movsxd 修复; 原 c198c445 是修复前镜像)  # 2026-08-31 站A+B+C + typedef double + cast32 修复 (csmith s61):
                         # cast32 修复后中文链 4-step 收敛 (v4..v7 稳定; v1 种子 transient)
                         # cast32: 32 位类型 cast 标记表 + nll 清零 (uint32<int64 比较误判无符号根治)
                         # (en 链同步 2DED3F47, 均种子修复)  # 2026-08-31 中文前端语法独立 站A+B+C + typedef double 局部声明修复 (种子层):
                         # implicit_cast_check rsp_off 越界修复 + typedef double 类型标记传导
                         # (en 链同步 BC0E5B22, 均为种子修复)  # 2026-08-31 中文前端语法独立 站A+站B+站C (函数/变量/常量 + 控制词无括号 + 类型别名):
                         # 站C: 类型 名 = 基础类型; (整/双/长/短/字/无 字/长 长 — 型定加+左系 注册)
                         # (sinicize 层注入, zh 独有, en 链 BF89F1E8 未动)
                         # 中文自举链不动点 (v2..v5; v1 是种子 transient)
                         # 批4: #define→#定义词/#include→#包含/#ifdef→若有定义/#ifndef→若无定义/#endif→结束若
                         # 用户程序可用中文指令词 (pp 各阶段双认, C 头英文指令保留)
                         # 踩坑链: ①指令判断双认但读名字偏移硬编码英文长度 (寻二+5/6) → 跳指令词修;
                         #  ②边界替换 [6] 与 +6] 两种形态; ③expand/collect/副组 逐字符 define 判断;
                         #  ④毛简 (pp_def_parse) 寻+=6; ⑤函数宏读名字循环缺 0x80 (中文宏名);
                         #  ⑥「预处理行不展开」守卫只认英文首字符 (d/u/i/e/l/p/n) → +0x80;
                         #  ⑦正则先带!后不带! + 负向断言防嵌套膨胀 (LABEL-OVERFLOW)
                         # 验证: 中文指令词程序 (对象宏/函数宏/若有定义/包含/结束若) OK; 功能全绿

Write-Host "== [1/2] 生成 qcc_zh.jy =="
if ($Regenerate) {
    python scripts/sinicize.py
    if ($LASTEXITCODE -ne 0) { throw "sinicize 失败" }
} else {
    # fix 2026-08-31 中文主源模式: qcc_zh.jy 是主源, 特性直接写中文端, sinicize 不再覆盖
    # (英文源修复需同步时, 手动跑 sinicize.py 重建后重做中文特性; 或显式 -Regenerate)
    Write-Host "  [主源] 使用现有 srclib_jiayan/qcc_zh.jy (跳过 sinicize; 如需重建加 -Regenerate)"
    if (-not (Test-Path srclib_jiayan\qcc_zh.jy)) { throw "qcc_zh.jy 不存在, 请先 -Regenerate 生成" }
}

Write-Host "== [2/2] 中文自举链 v1..v7 =="
Remove-Item qcc_zh_v1.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v2.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v3.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v4.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v5.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v6.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v7.exe -ErrorAction SilentlyContinue

& .\qcc_x86.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v1.exe
if ($LASTEXITCODE -ne 0) { throw "种子编译 qcc_zh.jy 失败" }
& .\qcc_zh_v1.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v2.exe
& .\qcc_zh_v2.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v3.exe
& .\qcc_zh_v3.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v4.exe
& .\qcc_zh_v4.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v5.exe
& .\qcc_zh_v5.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v6.exe
& .\qcc_zh_v6.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v7.exe

$h1 = (Get-FileHash qcc_zh_v1.exe -Algorithm SHA256).Hash
$h2 = (Get-FileHash qcc_zh_v2.exe -Algorithm SHA256).Hash
$h3 = (Get-FileHash qcc_zh_v3.exe -Algorithm SHA256).Hash
$h4 = (Get-FileHash qcc_zh_v4.exe -Algorithm SHA256).Hash
$h5 = (Get-FileHash qcc_zh_v5.exe -Algorithm SHA256).Hash
$h6 = (Get-FileHash qcc_zh_v6.exe -Algorithm SHA256).Hash
$h7 = (Get-FileHash qcc_zh_v7.exe -Algorithm SHA256).Hash
Write-Host "v1=$($h1.Substring(0,8))"
Write-Host "v2=$($h2.Substring(0,8))"
Write-Host "v3=$($h3.Substring(0,8))"
Write-Host "v4=$($h4.Substring(0,8))"
Write-Host "v5=$($h5.Substring(0,8))"
Write-Host "v6=$($h6.Substring(0,8))"
Write-Host "v7=$($h7.Substring(0,8))"

# fix 2026-08-31: cast32 修复后中文链多代收敛 (v1->v2->v3->v4 稳定), 判据 = 最后三代收敛 (v5==v6==v7)
if (($h5 -eq $h6) -and ($h6 -eq $h7) -and ($h5.Substring(0,8) -eq $expected)) {
    Write-Host "[PASS] 中文自举链不动点 $expected 达成 (v5..v7 收敛; v1=$($h1.Substring(0,8)) 种子 transient)" -ForegroundColor Green
    exit 0
} else {
    Write-Host "[FAIL] 中文链未收敛到 $expected (实际 v5=$($h5.Substring(0,8)))" -ForegroundColor Red
    exit 1
}
