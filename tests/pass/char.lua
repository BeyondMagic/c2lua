os.exit((function(args)
	local a = 65
	local b = (a + 1)
	local sum = (a + b)
	print(string.format("%d %d %d", a, b, sum))
	return 0
end)(arg))
