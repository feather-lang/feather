# Test: regexp with match variable
regexp {f(..)bar} "foobar" match sub1
puts $match
puts $sub1
