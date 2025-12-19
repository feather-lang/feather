# Test: join flattens list by one level
set data {1 {2 3} 4 {5 {6 7} 8}}
puts [join $data]
