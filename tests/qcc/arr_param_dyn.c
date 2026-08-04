int get(int *r, int i) { return r[i]; }
int main() {
    int x[5];
    x[3] = 99;
    return get(x, 3) - 99;
}
