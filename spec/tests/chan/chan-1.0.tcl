# Test: chan names returns list containing standard channels
puts [expr {"stdin" in [chan names]}]
puts [expr {"stdout" in [chan names]}]
puts [expr {"stderr" in [chan names]}]
