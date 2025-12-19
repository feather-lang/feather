# Test: info procs - does not include builtins
puts [expr {"set" in [info procs]}]
