int main() {
    int *p;
    int i;
    p = malloc(32);
    for (i = 0; i < 8; i = i + 1) { p[i] = i * 3; }
    return p[7] - 21;
}
