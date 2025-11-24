os.exit((function(args)
	local arr = { 10, 20, 30 }
	arr[2] = 99
	local x = arr[1]
	return x
end)(arg))
