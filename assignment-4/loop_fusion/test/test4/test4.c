int test_dependence() {
    int A[100];
    int N = 100;
    for (int i = 1; i < N; i++) {
        A[i] = A[i-1] + 1;  // Dipendence from A[i - 1]
    }
    for (int i = 1; i < N; i++) {
        A[i-1] = A[i+3] * 2;  // Dipendence from A[i]
    }
    return 0;
}