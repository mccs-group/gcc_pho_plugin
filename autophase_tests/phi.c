extern int rand();
extern int print(int a, int b, int c);
extern int calc(int a);
extern int wow(int* ptr);

void check_stmt(int a)
{
    return;
}

int main()
{
    int a;
    int b = rand();
    int *ptr = 0;

    if (b > 20)
    {
        a = 5;
        ptr = &b;
    }
    else
    {
        a = 8;
        ptr = &a;
    }

    int d = calc(5);
    // b = calc(a);

    // if ( rand() == 100 )
    //     goto jump_label;

    int c = a ^ b;
    calc(*ptr);
    wow(&a);
    print(a, b, c);

jump_label:

    return a;
}