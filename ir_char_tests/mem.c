extern int rand();
extern int print(int a, int b);
extern int calc(int a);
extern int wow(int* ptr);

int b;
int glob;

int main()
{
    b = rand();
    int a = rand();
    int* ptr = &glob;

    if (b > 20)
    {
        b = 20;
        glob = calc(10);
    }
    else
    {
        b = 15;
        wow(ptr);
    }

    print(glob, b);

    return b;
}