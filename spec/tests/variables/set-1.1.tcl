# Test: basic set and get
# Set creates variable, returns value

set x hello
puts $x
set y [set x]
puts $y
