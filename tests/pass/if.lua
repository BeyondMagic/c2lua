os.exit((function(args)
	local a = 1
	if (a == 1) then
		do
			print(string.format("numero: %d", a))
		end
	end
	do
		print(string.format("verdadeiro: %d", a))
	end
	print("fim")
	return 0
end)(arg))
