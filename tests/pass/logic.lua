os.exit((function(args)
	local a = 1
	local b = 0
	local c = (((a ~= 0) and not ((b ~= 0))) and 1 or 0)
	local d = (((b ~= 0) or (a ~= 0)) and 1 or 0)
	print(string.format("logic %d %d", c, d))
	return 0
end)(arg))
