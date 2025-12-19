# Test: after idle executes when event loop is idle
set done 0
after idle {set done 1}
vwait done
puts $done
