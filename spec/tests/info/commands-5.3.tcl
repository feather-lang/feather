# Test: info commands - includes user-defined procs
proc myproc {} { return 1 }
puts [expr {"myproc" in [info commands]}]
