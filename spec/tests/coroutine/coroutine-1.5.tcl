# Test: yield without value returns empty string
proc emptyYield {} {
    yield
    yield
    return "finished"
}
coroutine ey emptyYield
puts "[ey]."
puts "[ey]."
