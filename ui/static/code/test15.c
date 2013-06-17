#include<stdio.h>

union Val{
    int i;
    char c;
};

int main()
{
    union Val test;
    test.i = 321;
    printf("%d %c", test.i, test.c);
    return 0;
}
