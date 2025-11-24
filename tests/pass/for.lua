os.exit((function(args)
	do
		local a = 10
		while ((a < 10) and (a > 1)) do
			do
				print(string.format("%d", a))
			end
			a = (a + 1)
		end
	end
	return 0
end)(arg))
