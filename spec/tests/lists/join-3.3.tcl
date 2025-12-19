# Test: join result is not a list (spaces become part of string)
set result [join {"a b" "c d"} :]
puts $result
