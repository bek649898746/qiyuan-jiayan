/* 形态族：窄化转型（截断 + 符号扩展/零扩展） */
int main(void) {
    long long big = 300;
    char c = (char)big;
    int i = (int)c;
    return i + 1;
}
