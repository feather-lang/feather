# Test: regsub with empty replacement
puts [regsub {foo} "foobar" ""]
puts [regsub -all {\d} "a1b2c3" ""]
