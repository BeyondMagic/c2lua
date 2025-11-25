int main()
{
	int a = 1;
	float b = 2.5;
	float c = a + b; // ok: int->float
	int x = 7;
	int y = 3;
	int z = x % y; // ok: % em inteiros
	printf("%f %d\n", c, z);
	return 0;
}