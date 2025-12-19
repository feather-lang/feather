# Test: after 0 fires quickly
set done 0
after 0 {set done 1}
vwait done
puts $done
