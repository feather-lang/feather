# Test: regexp -indices with submatches
regexp -indices {f(..)bar} "xfoobarx" match sub1
puts $match
puts $sub1
