/* 形态族：成员装载（非 printf 消费 —— `ir_form_member.c` 的 `s.a` 全在 printf 实参里，
   走内建线性流，掩盖了「载:成员」这一形态仍是空白的事实）。 */
struct S { int a; int b; };

int main(void) {
    struct S s;
    s.a = 3;
    s.b = 4;
    return s.a + s.b;
}
