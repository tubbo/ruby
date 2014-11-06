i = 0
foo = "literal"
while i<6_000_000 # benchmark loop 2
  i += 1
  foo == "literal"
end
