local function add(k, y)
	return (k + y)
end

local function sum(a, b)
	local r = ((a + b) / 10)
	return r
end

os.exit((function(args)
	local args_table = args
	local K = args_table and tonumber(args_table[1]) or 0
	local a = 10
	local b = 20
	local r = (sum(a, b) + add(a, b))
	return r
end)(arg))
