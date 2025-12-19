# Test: array names with -glob mode (explicit)
# Glob pattern matching

array set data {foo 1 foobar 2 food 3 bar 4}
puts [lsort [array names data -glob foo*]]
