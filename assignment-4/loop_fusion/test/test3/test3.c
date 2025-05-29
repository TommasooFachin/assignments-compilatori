void test_flowequivalent() {
    int A[50];
    int B[50];
    int N = 50;

    if (N > 0) {
        for (int i = 0; i < N; i++) {
            A[i] = i;
        }
    } else {
        for (int i = 0; i < N; i++) {
            B[i] = i * 2;
        }
    }
}