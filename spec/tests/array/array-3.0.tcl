# Test: array startsearch and nextelement
# Basic iteration through array elements

array set data {a 1 b 2 c 3}
set sid [array startsearch data]
set keys {}
while {[array anymore data $sid]} {
    lappend keys [array nextelement data $sid]
}
array donesearch data $sid
puts [lsort $keys]
