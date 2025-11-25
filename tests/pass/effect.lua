local function add(k, y)
	return (k + y)
end

os.exit((function(args)
	local args_table = args
	local K = args_table and tonumber(args_table[1]) or 0
	add(10, 20)
	return 0
end)(arg))
