#include<stdio.h>

int main()
{
    int a[2][3][2];
    int i, j, k;

    for (i=0;i<2;i++){
        for(j=0;j<3;j++){
            for(k=0;k<2;k++){
                a[i][j][k] = (i+1)*(j+1)*(k+1);
            }
        }
    }

    return 0;
}
