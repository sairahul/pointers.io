int N;


void f (int *b)

{

    int j;

    j = 3;

    b[j] = 15;

} /* f() */


int main (void)

{

    int i;
    int a[10];
    int *p;

    i = 5;
    N = 10;

    f(a);

    p = &i;

    *p = 20;

    return 0;

} /* main() */
