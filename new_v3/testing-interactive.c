#include<stdio.h>

int INC = 10;

int main()
{
    int arr[5] = {1,2,3,4,5};
    int *arr1[2];
    arr1[0] = &arr[0];
    arr1[1] = &arr[1];
    int i;
    int *p = &arr[0];
    for(i=0;i<5;i++){
        printf("arr[%d]: %d\n", i, arr[i]);
        *p = *p + INC;
        printf("arr[%d]: %d\n", i, arr[i]);
        p += 1;
    }
}