---
outline: deep
---

# Example: Turtle Graphics

A turtle graphics example showing how to embed Feather with image generation.

```bash
# Quick run
curl -L https://feather-lang.dev/turtle/main.go -o main.go
curl -L https://feather-lang.dev/turtle/go.mod -o go.mod
go run .
```

## Simple Version

Start with a minimal example that draws a shape and displays it:

```go
package main

import (
    "image"
    "image/color"
    "image/png"
    "math"
    "os"
    "os/exec"
    "runtime"

    "github.com/feather-lang/feather"
)

type Turtle struct {
    x, y    float64
    angle   float64
    penDown bool
    img     *image.RGBA
}

func NewTurtle(width, height int) *Turtle {
    img := image.NewRGBA(image.Rect(0, 0, width, height))
    for y := 0; y < height; y++ {
        for x := 0; x < width; x++ {
            img.Set(x, y, color.White)
        }
    }
    return &Turtle{
        x: float64(width) / 2, y: float64(height) / 2,
        angle: 90, penDown: true, img: img,
    }
}

func (t *Turtle) Forward(dist float64) {
    rad := t.angle * math.Pi / 180
    newX := t.x + dist*math.Cos(rad)
    newY := t.y - dist*math.Sin(rad)
    if t.penDown {
        drawLine(t.img, t.x, t.y, newX, newY, color.Black)
    }
    t.x, t.y = newX, newY
}

func (t *Turtle) Show() error {
    f, _ := os.CreateTemp("", "turtle-*.png")
    defer f.Close()
    png.Encode(f, t.img)
    cmd := "xdg-open"
    if runtime.GOOS == "darwin" { cmd = "open" }
    return exec.Command(cmd, f.Name()).Start()
}

func main() {
    interp := feather.New()
    defer interp.Close()

    turtle := NewTurtle(400, 400)

    interp.Register("forward", func(dist float64) { turtle.Forward(dist) })
    interp.Register("back", func(dist float64) { turtle.Forward(-dist) })
    interp.Register("left", func(deg float64) { turtle.angle += deg })
    interp.Register("right", func(deg float64) { turtle.angle -= deg })
    interp.Register("penup", func() { turtle.penDown = false })
    interp.Register("pendown", func() { turtle.penDown = true })
    interp.Register("show", func() error { return turtle.Show() })

    // Draw a square
    interp.Eval(`
        for {set i 0} {$i < 4} {incr i} {
            forward 100
            right 90
        }
        show
    `)
}

func drawLine(img *image.RGBA, x0, y0, x1, y1 float64, c color.Color) {
    dx, dy := math.Abs(x1-x0), math.Abs(y1-y0)
    sx, sy := 1.0, 1.0
    if x0 >= x1 { sx = -1 }
    if y0 >= y1 { sy = -1 }
    err := dx - dy
    for {
        img.Set(int(x0), int(y0), c)
        if math.Abs(x0-x1) < 1 && math.Abs(y0-y1) < 1 { break }
        e2 := 2 * err
        if e2 > -dy { err -= dy; x0 += sx }
        if e2 < dx { err += dx; y0 += sy }
    }
}
```

## Try Different Patterns

Replace the `interp.Eval(...)` call with different scripts:

Draw a star:

```tcl
for {set i 0} {$i < 5} {incr i} {
    forward 150
    right 144
}
show
```

Draw a spiral:

```tcl
for {set i 0} {$i < 36} {incr i} {
    forward [expr {$i * 5}]
    right 90
}
show
```

## Adding an Interactive REPL

To make it interactive, add a REPL loop that handles multi-line input:

```go
func main() {
    interp := feather.New()
    defer interp.Close()

    turtle := NewTurtle(400, 400)

    interp.Register("forward", func(dist float64) { turtle.Forward(dist) })
    interp.Register("back", func(dist float64) { turtle.Forward(-dist) })
    interp.Register("left", func(deg float64) { turtle.angle += deg })
    interp.Register("right", func(deg float64) { turtle.angle -= deg })
    interp.Register("penup", func() { turtle.penDown = false })
    interp.Register("pendown", func() { turtle.penDown = true })
    interp.Register("show", func() error { return turtle.Show() })

    fmt.Println("Turtle Graphics REPL")
    fmt.Println("Commands: forward N, back N, left N, right N, penup, pendown, show")

    scanner := bufio.NewScanner(os.Stdin)
    var buffer strings.Builder
    fmt.Print("\nturtle> ")

    for scanner.Scan() {
        if buffer.Len() > 0 { buffer.WriteString("\n") }
        buffer.WriteString(scanner.Text())

        input := buffer.String()
        result := interp.Parse(input)

        switch result.Status {
        case feather.ParseIncomplete:
            fmt.Print("   ...> ")
            continue
        case feather.ParseError:
            fmt.Println("Parse error:", result.Message)
        case feather.ParseOK:
            if strings.TrimSpace(input) != "" {
                res, err := interp.Eval(input)
                if err != nil {
                    fmt.Println("Error:", err)
                } else if res.String() != "" {
                    fmt.Println(res.String())
                }
            }
        }
        buffer.Reset()
        fmt.Print("turtle> ")
    }
}
```

Now you can draw interactively:

```
turtle> forward 100
turtle> right 90
turtle> forward 100
turtle> show
```

Multi-line input is supported - the REPL waits for complete commands:

```
turtle> for {set i 0} {$i < 4} {incr i} {
   ...>     forward 100
   ...>     right 90
   ...> }
turtle> show
```

![Square drawn by turtle](/turtle/square.png)
