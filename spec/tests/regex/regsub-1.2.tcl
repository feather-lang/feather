# Test: regsub -all option
puts [regsub -all {o} "foobar" "X"]
puts [regsub -all {\d} "a1b2c3" "X"]
