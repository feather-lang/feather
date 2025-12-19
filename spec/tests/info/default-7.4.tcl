# Test: info default - nonexistent parameter
proc someproc {a b} { return 1 }
catch {info default someproc z defval} err
puts $err
