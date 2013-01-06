#include<stdio.h>

int main()
{
    int a[2][3];
    int i, j;

    for (i=0;i<2;i++){
        for(j=0;j<3;j++){
            a[i][j] = (i+1)*(j+1);
        }
    }

    return 0;
}
