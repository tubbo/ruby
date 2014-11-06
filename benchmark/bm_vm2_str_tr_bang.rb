i = 0
str = "a"
while i<6_000_000 # benchmark loop 2
  i += 1
  str.tr!("a", "A")
  str.tr!("A", "a")
end
