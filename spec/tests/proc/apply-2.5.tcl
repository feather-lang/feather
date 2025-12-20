# Test: apply - scope isolation (lambda does not see caller's local variables)
proc testscope {} {
    set localvar 100
    catch {apply {{} {set localvar}}} result
    return $result
}
puts [testscope]
