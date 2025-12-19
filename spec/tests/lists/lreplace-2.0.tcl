# Test: lreplace wrong args
catch {lreplace {a b c}} msg
puts $msg
