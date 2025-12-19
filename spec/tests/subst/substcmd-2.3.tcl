# Test: subst with break exception
# Break stops substitution of rest of string

puts [subst {abc,[break],def}]
puts [subst {start [break] end}]
