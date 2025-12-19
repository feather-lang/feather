# Test: dict for - continue
set d [dict create a 1 b 2 c 3]
dict for {k v} $d {
    if {$k eq "b"} continue
    puts "$k=$v"
}
