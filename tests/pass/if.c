int main()
{
	int a = 1;
	int b = 2;
	if (a == 1)
	{
		printf("numero: %d\n", a);
	}
	if (1)
	{
		printf("verdadeiro: %d\n", a);
	}
	if (0)
	{
		printf("falso: %d\n", a);
	}
	if (a != 1)
		if (b == 2)
			printf("numero diferente: %d\n", a);
		else
			printf("numero estranho: %d\n", a);
	else
		printf("numero igual: %d\n", a);
	puts("fim");
	return 0;
}