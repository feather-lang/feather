# Test: file channels lists standard channels
set chans [file channels]
puts [expr {"stdin" in $chans}]
puts [expr {"stdout" in $chans}]
