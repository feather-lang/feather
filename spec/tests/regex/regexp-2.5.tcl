# Test: regexp optional submatch (when group doesn't match)
regexp {foo(bar)?baz} "foobaz" match sub1
puts $match
puts "sub1=<$sub1>"
