# Test: array names with -exact mode
# Matches only exact string

array set data {foo 1 foobar 2 food 3}
puts [lsort [array names data -exact foo]]
