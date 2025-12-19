# Test: regsub with numbered back-references
puts [regsub {(\w+)@(\w+)} "user@host" {\2-\1}]
