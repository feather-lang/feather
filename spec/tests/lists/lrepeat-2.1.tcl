# Test: lrepeat negative count error
catch {lrepeat -1 a} msg
puts $msg
