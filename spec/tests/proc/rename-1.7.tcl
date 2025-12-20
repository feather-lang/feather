# Test: chain of renames
proc original {} { return "original" }
rename original step1
rename step1 step2
rename step2 final
puts [final]
