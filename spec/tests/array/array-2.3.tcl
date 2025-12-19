# Test: array names with -regexp complex pattern
# Match keys ending with digit

array set data {item1 a item2 b item c noitem 4}
puts [lsort [array names data -regexp {\d$}]]
