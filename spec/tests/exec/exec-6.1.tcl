# Test: exec with variable expansion
# Tcl variable expanded before exec

set cmd echo
set arg "hello from var"
puts [exec $cmd $arg]
