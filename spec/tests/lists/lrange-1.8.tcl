# Test: lrange vs lindex for braced elements
set var {some {elements to} select}
puts [lindex $var 1]
puts [lrange $var 1 1]
