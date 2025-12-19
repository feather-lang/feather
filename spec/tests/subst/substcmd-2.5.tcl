# Test: subst with return exception
# Return substitutes the returned value

puts [subst {abc,[return foo],def}]
puts [subst {start [return bar] end}]
