# Test: info exists checks variable existence
# Returns 1 if exists, 0 if not

puts [info exists undefined]
set defined value
puts [info exists defined]
