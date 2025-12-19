# Test: exec with stderr output
# stderr causes error by default

catch {exec sh -c "echo error >&2"} result
puts [string match "*error*" $result]
