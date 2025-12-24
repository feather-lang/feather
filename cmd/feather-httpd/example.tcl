# Example feather-httpd configuration
#
# Run with: ./feather-httpd example.tcl

# Simple hello world route
route GET / {
    header Content-Type text/plain
    response "Hello from feather-httpd!"
}

# Echo the request path
route GET /echo {
    set path [request path]
    response "You requested: $path"
}

# JSON response example
route GET /json {
    header Content-Type application/json
    response {{"status": "ok", "message": "feather is running"}}
}

# Query parameter example
route GET /greet {
    set name [request query name]
    if {$name eq ""} {
        set name "World"
    }
    response "Hello, $name!"
}

# Custom status code example
route GET /notfound {
    status 404
    response "This page intentionally returns 404"
}

# Start the server
listen 8080
