#include <stdarg.h>
#include <stdio.h>

int a = 0, d = 0, e = 0;
int b = 3, c = 2;

int main() {

    while(1) {
        if(a==2){
            a = b + c;
        } else {
            e = 3;  
            if (d==6) {
                break;
            }
        }
        d = a + 1;
    }

}
