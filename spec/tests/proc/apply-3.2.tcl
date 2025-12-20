# Test: apply - modifying global variable inside lambda
set counter 0
apply {{} {global counter; incr counter}}
apply {{} {global counter; incr counter}}
apply {{} {global counter; incr counter}}
puts $counter
