#include <stdio.h>

void test_code_motion() {
    int a = 0, b = 3, c = 2;
    int d = 0, e = 0, f = 0, g = 10;

    while (1) {
        int x = b + c;  // loop-invariant e candidata per la code motion

        f++;
        int y = a + 1; // loop-invariant e candidata per la code motion

        if (a == 5)
            d = 42;

        e = 7;
        if (a % 2 == 0)
            e = 8;

        f = y + 1; // non domina i suoi usi

        int z = 100;  

        if (g > 5)  
            g = z + 1; // non domina i suoi usi

        // a++;
    }
}
