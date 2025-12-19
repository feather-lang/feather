# Test: expr string greater than (gt)
puts [expr {"abd" gt "abc"}]
puts [expr {"abc" gt "abc"}]
puts [expr {"abb" gt "abc"}]
puts [expr {"a" gt "A"}]
