# Test: upvar multiple variable pairs
proc swapvars {name1 name2} {
    upvar 1 $name1 v1 $name2 v2
    set temp $v1
    set v1 $v2
    set v2 $temp
}
set a "first"
set b "second"
swapvars a b
puts "a = $a"
puts "b = $b"
