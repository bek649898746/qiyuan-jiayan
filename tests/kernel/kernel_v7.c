// 甲言内核 v7 — Gate 3 Agent 类型 (内联版，无 #include)
// 构建: qcc_x86_new.exe -bin tests/kernel/kernel_v7.c -o scratch_test/kernel_v7.bin

typedef struct {
    整 kv_slot;
    整 token_budget;
    整 state;
    整 gpu_id;
    整 capabilities;
    整 dep_count;
    整 deps[4];
} Agent;

// === 串口 ===
空 serial_init(整 c) {
    outb(c+1,0); outb(c+3,0x80); outb(c+0,1); outb(c+1,0);
    outb(c+3,3); outb(c+2,0xC7); outb(c+4,0x0B);
}
空 serial_wait(整 c) { 循环((inb(c+5)&0x20)==0){} }
空 spc(整 c,整 v) { 若(v==10){serial_wait(c);outb(c,13);} serial_wait(c);outb(c,v); }
空 sps(整 c,字*s) { 循环(*s!=0){spc(c,*s);s++;} }

// === Agent 输出 (简化: 只显示 slot) ===
空 agent_show(整 c, Agent *a, 字* name) {
    sps(c, name);
    整 s = a->kv_slot;
    若 (s == 0) sps(c, " slot=0\n");
    若 (s == 1) sps(c, " slot=1\n");
    若 (s == 2) sps(c, " slot=2\n");
    若 (s == 3) sps(c, " slot=3\n");
}

// === 入口 ===
空 _start(空) {
    整 c = 0x3F8;
    serial_init(c);

    sps(c, "JIAYAN v7 GATE3\n");
    sps(c, "AGENT TYPE | SEED:828\n");

    // 四个 Agent 实例 (栈上)
    Agent 思考; Agent 代码; Agent 审查; Agent 测试;

    // 思考引擎
    思考.kv_slot = 0; 思考.token_budget = 128;
    思考.state = 0; 思考.gpu_id = 0;
    思考.capabilities = 9;
    思考.dep_count = 0;

    // 代码Agent
    代码.kv_slot = 1; 代码.token_budget = 4096;
    代码.state = 0; 代码.gpu_id = 0;
    代码.capabilities = 7;
    代码.dep_count = 1; 代码.deps[0] = 0;

    // 审查Agent
    审查.kv_slot = 2; 审查.token_budget = 1024;
    审查.state = 0; 审查.gpu_id = 0;
    审查.capabilities = 1;
    审查.dep_count = 1; 审查.deps[0] = 1;

    // 测试Agent
    测试.kv_slot = 3; 测试.token_budget = 0;
    测试.state = 0; 测试.gpu_id = 0;
    测试.capabilities = 4;
    测试.dep_count = 1; 测试.deps[0] = 1;

    // 输出
    agent_show(c, &思考, "think");
    agent_show(c, &代码, "code");
    agent_show(c, &审查, "review");
    agent_show(c, &测试, "test");

    sps(c, "PIPE: think->code->review+test\n");
    循环(1){__asm(0xF4);}
}
