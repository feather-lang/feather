# Test: exec with list expansion
# Use {*} to expand args

set args {one two three}
puts [exec echo {*}$args]
