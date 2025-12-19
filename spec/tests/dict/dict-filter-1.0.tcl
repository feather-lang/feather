# Test: dict filter key - basic pattern matching
set d [dict create apple 1 banana 2 apricot 3 cherry 4]
puts [dict filter $d key a*]
puts [dict filter $d key *a*]
