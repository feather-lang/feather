# Test: info procs - list includes user-defined proc
proc myproc1 {} { return 1 }
puts [expr {"myproc1" in [info procs]}]
