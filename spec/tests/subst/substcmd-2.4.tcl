# Test: subst with continue exception
# Continue substitutes empty string for that substitution

puts [subst {abc,[continue],def}]
puts [subst {start [continue] end}]
