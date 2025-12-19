# Test: regexp -all option - count matches
puts [regexp -all {o} "foobar"]
puts [regexp -all {x} "foobar"]
puts [regexp -all {\d} "a1b2c3d4"]
