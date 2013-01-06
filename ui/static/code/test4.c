#include<stdio.h>

int main()
{
  int a[4] = {1, 2, 3, 4};
  for(int i=0;i<4;i++){
    printf("%d\n", a[i]);
    a[i] = a[i] + 10;
    printf("%d\n", a[i]);
  }
  return 0;
}
