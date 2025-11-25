os.exit((function(args)
	local a = 1
	local b = 2.5
	local c = (a + b)
	local x = 7
	local y = 3
	local z = (x % y)
	print(string.format("%f %d", c, z))
	return 0
end)(arg))
