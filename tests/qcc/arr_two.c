int main() {
    int r[5];
    int s[5];
    int i;
    i = 0;
    while (i < 5) { r[i] = i; s[i] = i * 2; i = i + 1; }
    return (r[4] + s[4]) - 12;
}
