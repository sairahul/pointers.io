#include<stdio.h>

struct ABC{
    int a;
    char c[4];
};

int main(){
    struct ABC a;
    a.a = 10;
    a.c[0] = 'A';

    return 0;
}
