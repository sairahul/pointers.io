#include<stdio.h>

union ABC 
{
    int x;
    char y;
};

int main()
{
    union ABC a;
    a.x = 321; 
    printf("%d-%d", a.x, a.y);
    return 0;
}