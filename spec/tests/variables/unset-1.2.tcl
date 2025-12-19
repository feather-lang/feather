# Test: unset multiple variables
# Can unset several variables at once

set a 1
set b 2
set c 3
unset a b
puts [info exists a]
puts [info exists b]
puts [info exists c]
