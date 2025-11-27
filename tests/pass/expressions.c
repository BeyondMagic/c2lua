int add(int k, int y)
{
    return k + y;
}

int sum(int a, int b)
{
    // a + b
    /*
     * comment
     */
    int r = (a + b) / 10;
    return r;
}

int main(int K)
{
    int a = 10;
    int b = 20;
    int r = sum(a, b) + add(a, b);
    return r;
}
