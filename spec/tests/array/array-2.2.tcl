# Test: array names with -regexp mode
# Regular expression matching

array set data {foo 1 foobar 2 food 3 bar 4 baz 5}
puts [lsort [array names data -regexp {^foo}]]
