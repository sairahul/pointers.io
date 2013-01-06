#include<stdio.h>

struct ValZ{
    int i;
    char c;
};

int main()
{
    struct ValZ test;
    test.i = 10;
    test.c = 'A';
    printf("%d %c", test.i, test.c);
    return 0;
}
