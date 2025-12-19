# Test: dict for - basic iteration
set d [dict create a 1 b 2 c 3]
dict for {k v} $d {
    puts "$k=$v"
}
