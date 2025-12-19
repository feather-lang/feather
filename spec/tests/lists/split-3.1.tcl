# Test: split roundtrip with join
set original "a:b:c:d"
set parts [split $original :]
set rejoined [join $parts :]
puts $rejoined
