# Test: regsub empty string and empty pattern
puts [regsub {} "foobar" "X"]
puts [regsub {foo} "" "X"]
