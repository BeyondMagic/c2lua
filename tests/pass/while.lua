os.exit((function(args)
	local a = 0
	while (a < 10) do
		do
			print(string.format("%d", a))
			a = (a + 1)
		end
	end
	return 0
end)(arg))
