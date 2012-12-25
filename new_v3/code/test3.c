#include<stdio.h>

void recursion(int i)
{
    if (i<0)
        return;
    printf("%d", i);
    recursion(i-1);
}

int main()
{
    int TEST = 5;
    recursion(TEST);
    return 0;
}
