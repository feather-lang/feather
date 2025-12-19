# Test: info globals - pattern matching
set testglobal1 1
set testglobal2 2
set other 3
puts [lsort [info globals testglobal*]]
