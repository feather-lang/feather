# Test: linsert wrong args
catch {linsert {a b c}} msg
puts $msg
