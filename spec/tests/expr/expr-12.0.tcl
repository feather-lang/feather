# Test: expr in operator with command substitution
set mylist {a b c}
puts [expr {"a" in $mylist}]
puts [expr {"d" in $mylist}]
