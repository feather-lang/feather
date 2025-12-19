# Test: lset nested index
set x {{a b c} {d e f} {g h i}}
puts [lset x 1 1 foo]
