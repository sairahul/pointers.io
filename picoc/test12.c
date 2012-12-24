#include<stdio.h>

struct D{
    int d;
};

struct ABC{
    int a;
    struct D d;
};

int main(){
    struct ABC a[2];
    a[0].a = 10;

    a[1].a = 20;
    return 0;
}
