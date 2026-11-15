/* 缺陷复现件（P0 · 编译器崩溃）：函数指针变量 —— IR 模式 `0xC0000005`
   复现：QCC_CODEGEN=ir qcc_zh_v2.exe -c scripts/probe/bugs/ir_fptr_var_crash.c -o x.o
   实测（2026-11-10）：
     legacy  rc=0
     IR      rc=3221225477 (0xC0000005)   ← 编译器自身崩，不是产物问题
   既有性：老对照组 build/control_qcc_8CDD3C48.exe 的 IR 模式同样崩 ⟹ 并非刀九十一/九十二 引入
           （刀九十二 只把「间接调用」改成响亮 UNIMPL，但崩在更早的发射/记录阶段）。
   ⚠️ 本文件**不进任何判据**（避免判据一跑就触发崩溃风暴）；修好后请把它加进
      `scripts/check_ir_silent_neg.py` 的 CASES 并保持 SAME。 */
int f(int *p) { return *p; }

int g(void) { int (*q)(int *) = f; return q(0) + 0; }
