require 'set'
set = Set.new
i = 0
while i<6_000_000 # while loop 2
  i += 1
  set.include?("foo")
end
