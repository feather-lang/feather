# Test: after ms script with vwait - timer fires and sets variable
set done 0
after 10 {set done 1}
vwait done
puts $done
