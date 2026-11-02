int hot_mulrow(int *a, int *b, int n) {
    int s = 0;
    for (int k = 0; k < n; k++) s += a[k] * b[k];
    return s;
}
