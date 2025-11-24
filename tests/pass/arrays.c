int main()
{
	int arr[3] = {10, 20, 30};
	arr[1] = 99; // Lua: arr[2] = 99
	int x = arr[0];
	return x;
}