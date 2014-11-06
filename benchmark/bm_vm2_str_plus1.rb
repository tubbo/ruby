i = 0
foo = "a"
while i<6_000_000 # benchmark loop 2
  i += 1
  foo + "b"
end
