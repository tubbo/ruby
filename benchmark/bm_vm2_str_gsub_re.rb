i = 0
str = ""
while i<6_000_000 # benchmark loop 2
  i += 1
  str.gsub(/a/, "")
end
