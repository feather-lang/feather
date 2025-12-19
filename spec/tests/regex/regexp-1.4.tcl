# Test: regexp -indices option
regexp -indices {foo} "abcfoodef" match
puts $match
