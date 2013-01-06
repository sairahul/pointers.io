#include<stdio.h>

struct key {
    char c;
    int count;
};

int main(){
    struct key keyarr[3];
    keyarr[0].c = 'A';
    keyarr[0].count = 10;

    keyarr[1].c = 'B';
    keyarr[1].count = 11;

    keyarr[2].c = 'C';
    keyarr[2].count = 12;

    return 0;
}
