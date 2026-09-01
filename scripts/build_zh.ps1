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
$expected = '13C894CA'  # 2026-09-01 阶段C-1 C23 char8_t UTF-8 字符: 变量声明兜底 (uint8_t 同语义 1字节无符号) + sizeof 三处 (L5446 变量分支兜底/L5563 标量分支/L5574 型定<0 兜底) + sizeof 变量分支 标准类型名兜底; 原 D63EBDA3  # 2026-09-01 阶段C-1 C23 枚举底层类型: 枚 名 : 类型 { } 枚底表 (枚底名+字符/短/长/无符号) + 顶层 L13348/块内 L7016 冒号解析记录 + 枚举变量按底层宽度注册 + sizeof 枚底查询 (L5500); 原 8FA9A458  # 2026-09-01 阶段C-1 C23 typeof 类型查询: typeof(x) y = init; 复制变量/类型 类型标志 (变量查询函数+类型关键字兜底; 类型取 双认); 原 959068FF  # 2026-09-01 阶段C-1 C23 nullptr 空指针字面量: nullptr/空指针 双认 (原级 与 NULL 同值0节点, 赋任意指针+与指针比较+条件); 原 73282B12  # 2026-09-01 阶段C-1 #193 单继承: 构 子 : 父 { } 基类字段置首复制 (正聚原样复制零漂移) + 调用 分派父结构体链回退 (方法继承) + 负例 A9004父未声明/A9005联合不可继承; 原 29A088DC  # 2026-09-01 阶段C-1 #197 封装/访问控制完善: 私有/公开 前置+后置标记全修 (①函数定义 L14429 保留私有标记 ②变量逆酉设/清而非是否静态标 ③全局注册5处查私有集合); 原 2D6CC486  # 2026-09-01 阶段C-1 #36+#37 生命周期: 借用 存期 _ x; 匿名域 (通配不查域) + 存期 a: b; 域约束 (父域必须已声明 A3005); 原 5A111A8A  # 2026-09-01 阶段C-1 #68 背压: 背压 送 q, v; — 通道满时发送阻塞 (Go 有界通道语义, 自旋等待 循环(__ch_q_c>=__ch_q_cap){} + 内联入队); 原 6EDB76D2  # 2026-09-01 阶段C-1 #115 trait 继承: 接口 子 : 父 { 方法 } 合并父方法 + 同名覆盖 + 调用分发回溯父接口实现绑定; A9004 父接口未声明; 坑: 语句() 有两处接口分支 (L8500 真/L8548 死代码), 修改必须落在 L8500; 原 643A912C  # 2026-09-01 阶段C-1 #116 impl Trait (二步 返回检查): return 变量类型必须实现 接口 (遍历 实现 表 宽失/旁从); A9005 拦截; 坑: 查函数有注册副作用→改遍历函数表; 原 3B19FCEE  # 2026-09-01 阶段C-1 #116 impl Trait (第一步 落IR): 函数 名() -> 实现 接口名 返回匿名实现类型 (箭头 token 语法); 函数表加 返接口 字段; vt_tyvar/空对 写接口索引+1; A9004 未声明接口拦截; 原 1F03F219  # 2026-09-01 阶段C-1 #4 生命周期标注 (二步 存期 域): 存期 a; 声明生命周期域 + 借用 存期 a x; 域绑定 → vt_lifetime_id/源粗 写域编号 (1..N); A3005 未声明域拦截; 块() 未知类型排除表加 存期 (原被吞当类型声明); 原 19ACA59B  # 2026-09-01 阶段C-1 #4 生命周期标注 (第一步 落IR): 借用状态机写入 vt_lifetime/总子 (2=不可变借用 3=可变借用 0=放还) + vt_lifetime_id/源粗 域编号 + 借用表压缩同步; 行为不变 (借用族 5 测试过); 原 84D61F19  # 2026-09-01 阶段C-0 IR 激活 (宏门 前亥一=1): 预埋结构位真实落位 (BSS 分离加载, 产物文件不变, 编译逻辑不变); 原 E96C7718  # 2026-09-01 阶段B 报错消息中文化收尾 (15 处用户可见英文→中文: csym/crel/paren_balance×4/at label/OOM×2/cannot×3/parse failed/annotation/I-O error; 保留 [ERR] 标签/A码/关键字/编译器名 qcc_x86) 后新不动点; 原 46dead87  # 2026-09-01 #231 repr 控制: 打包 构 { } 紧凑布局 (字段无对齐填充, 尺寸不轮 up); 原 2029a6ce  # 2026-09-01 #218 Maybe/Result 链: ?. 结构体值左操作数走普通 . 访问 (遍历变量表判断指深 — 查变量返回栈偏移非表索引); 原 6776247c  # 2026-09-01 #173 注解/属性完整: 注解 名=值; 顶层元数据标注 + --annotations 导出 (查注解 编译期查询); 原 96cd20e5  # 2026-09-01 #66 定时器/超时: 延时 N→timer_delay(N) / 超时 N→timer_deadline(N) 语法糖脱糖; 原 2b2e605e  # 2026-09-01 #181 宏卫生深化: #定义词 宏注册读名偏移条件化 (中文 9 字节, 原硬编码 +6 致中文宏不展开) + #未定义 双认; 原 1f5e68fb  # 2026-09-01 #289 DWARF 调试信息 (-g): PE 5 节 + .debug_abbrev/.debug_info/.debug_line 长节名 + 符号表/字符串表; 隔离实证: DWARF前后编译器无-g 产物 .text 逐字节一致; 原 e8258f76  # 2026-09-01 #279 名称空间 memmove 修复后新不动点 (删词元 复制字符串→移动内存 防重叠); 原 3819d542  # 2026-09-01 #279 名称空间/模块完整: 词元级 pass (块内符号加前缀 名_, 名::符号 合并) (Vec 迭代): 遍 x 于 v 支持 动态数组 — 递增节点修复 (加节点漏 __vi 操作数 → __vi 恒=2 死循环; 改 建节(23) 增量节点) + 复制长度 64→32  # 2026-08-31 #255 顶层重载修复 (主循环 L12325 加 重载 分支; 原顶层重载被变量声明吞 → main 丢失)  # 2026-08-31 中文化去吞 #105 枚举带负载  # 2026-08-31 中文化去吞 #105 枚举带负载 (变体(类型) 识别: 注册变体值+跳过类型参数, 不泄漏为成员)  # 2026-08-31 中文化去吞 #23 作用域守卫基础版  # 2026-08-31 中文化去吞 #23 作用域守卫基础版 (扫尾 = 函数退出执行, defer 路径; 块级待深化)  # 2026-08-31 中文化去吞 #136 通配符模式 _  # 2026-08-31 中文化去吞 #136 通配符模式 _ (池平=2 恒真比较, 匹配任何值不绑定)  # 2026-08-31 中文化去吞 #163 finally  # 2026-08-31 中文化去吞 #163 finally (无论如何 = 函数退出无条件执行, defer 别名; 中文主源模式首个特性)  # 2026-08-31 深度中文化后新不动点 (string_sinicize: 错误消息正文中文化 OOM→内存不足/AST→抽象语法树; 保留 [ERR] 前缀/纯英文判断词/asm/符号模板; 原 46ef0edb 是注释中文化版)  # 2026-08-31 深度中文化后新不动点 (string_sinicize: 错误消息正文中文化 OOM→内存不足/AST→抽象语法树; 保留 [ERR] 前缀/纯英文判断词/asm/符号模板; 原 46ef0edb 是注释中文化版)  # 2026-08-31 补 signed 中文词 (有符号) 后新不动点 (signed char 中文可写: 有符号 字; kw/strcmp/cast 全链认)  # 2026-08-31 中文链 cast 类型解析中文等价注入修复  # 2026-08-31 中文链 cast 类型解析中文等价注入修复 (layer_change 变量形式词名 strcmp: 转型型/转型型二/中粗) + MAX_LABELS 262144 扩容后新不动点; 原 4a19764d 缺 cast 中文等价 → (无 长 长) cast 8B 标记丢失 → 64 位移位/比较全崩 (t1 c64=0 复现)  # 2026-08-31 无整中文语法缺口修复后新不动点 (sinicize KW_ALIAS unsigned→无整; 原 19277eb6 缺无整连写)  # 2026-08-31 镜像同步修复后新不动点 (qcc_zh.jy 重生成含 ll_ext32/p_esz/movsxd 修复; 原 c198c445 是修复前镜像)  # 2026-08-31 站A+B+C + typedef double + cast32 修复 (csmith s61):
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
