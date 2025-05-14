#include <stdio.h>

void test_function() {
    int i, j;
    int a[10], b[10];

    // Primo loop
    for (i = 0; i < 10; ++i) {
        a[i] = i;
    }

    // Secondo loop (adiacente)
    for (j = 0; j < 10; ++j) {
        b[j] = j;
    }
}
