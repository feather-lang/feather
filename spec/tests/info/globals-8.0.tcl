# Test: info globals - basic global variables
set gvar1 10
set gvar2 20
puts [expr {"gvar1" in [info globals]}]
puts [expr {"gvar2" in [info globals]}]
