# Test: exec 2>@1 merge stderr to stdout
# Redirect stderr to result

set result [exec sh -c "echo out; echo err >&2" 2>@1]
puts [llength [split $result \n]]
