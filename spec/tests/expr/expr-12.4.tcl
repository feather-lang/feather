# Test: expr in operator with proc-defined list
proc getlist {} { return {apple banana cherry} }
puts [expr {"banana" in [getlist]}]
puts [expr {"grape" in [getlist]}]
