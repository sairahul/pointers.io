#include<stdio.h>

int main(void)
{
    char *p1 = "char1";
    char *p2 = "Testing ABC";
    char *p3 = "India T";

    char *arr[3];

    arr[0] = p1;
    arr[1] = p2;
    arr[2] = p3;

   printf("\n p1 = [%s] %ld \n",p1, sizeof(p1));
   printf("\n p2 = [%s] %ld \n",p2, sizeof(p2));
   printf("\n p3 = [%s] %ld \n",p3, sizeof(p3));

   printf("\n arr[0] = [%s] \n",arr[0]);
   printf("\n arr[1] = [%s] \n",arr[1]);
   printf("\n arr[2] = [%s] \n",arr[2]);

   return 0;
}
