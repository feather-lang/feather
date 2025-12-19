# Test: regsub with & back-reference (entire match)
puts [regsub {\w+} "hello world" {[\0]}]
puts [regsub {\w+} "hello world" {[&]}]
