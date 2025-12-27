package main

import (
	"bufio"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"math"
	"os"
	"os/exec"
	"runtime"
	"strings"

	"github.com/feather-lang/feather"
)

type Turtle struct {
	x, y    float64
	angle   float64 // degrees, 0 = right, 90 = up
	penDown bool
	color   color.Color
	img     *image.RGBA
}

func NewTurtle(width, height int) *Turtle {
	img := image.NewRGBA(image.Rect(0, 0, width, height))
	// Fill with white background
	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			img.Set(x, y, color.White)
		}
	}
	return &Turtle{
		x: float64(width) / 2, y: float64(height) / 2,
		angle: 90, penDown: true,
		color: color.Black, img: img,
	}
}

func (t *Turtle) Forward(dist float64) {
	rad := t.angle * math.Pi / 180
	newX := t.x + dist*math.Cos(rad)
	newY := t.y - dist*math.Sin(rad) // Y is inverted in images
	if t.penDown {
		drawLine(t.img, t.x, t.y, newX, newY, t.color)
	}
	t.x, t.y = newX, newY
}

func (t *Turtle) Show() error {
	f, err := os.CreateTemp("", "turtle-*.png")
	if err != nil {
		return err
	}
	defer f.Close()
	if err := png.Encode(f, t.img); err != nil {
		return err
	}
	// Open with system viewer
	cmd := "xdg-open"
	if runtime.GOOS == "darwin" {
		cmd = "open"
	}
	return exec.Command(cmd, f.Name()).Start()
}

func (t *Turtle) Clear() {
	bounds := t.img.Bounds()
	for y := bounds.Min.Y; y < bounds.Max.Y; y++ {
		for x := bounds.Min.X; x < bounds.Max.X; x++ {
			t.img.Set(x, y, color.White)
		}
	}
	t.x = float64(bounds.Max.X) / 2
	t.y = float64(bounds.Max.Y) / 2
	t.angle = 90
}

// drawLine uses Bresenham's algorithm
func drawLine(img *image.RGBA, x0, y0, x1, y1 float64, c color.Color) {
	dx := math.Abs(x1 - x0)
	dy := math.Abs(y1 - y0)
	sx, sy := 1.0, 1.0
	if x0 >= x1 {
		sx = -1
	}
	if y0 >= y1 {
		sy = -1
	}
	err := dx - dy
	for {
		img.Set(int(x0), int(y0), c)
		if math.Abs(x0-x1) < 1 && math.Abs(y0-y1) < 1 {
			break
		}
		e2 := 2 * err
		if e2 > -dy {
			err -= dy
			x0 += sx
		}
		if e2 < dx {
			err += dx
			y0 += sy
		}
	}
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
	interp.Register("clear", func() { turtle.Clear() })

	// Interactive REPL
	fmt.Println("Turtle Graphics REPL")
	fmt.Println("Commands: forward N, back N, left N, right N, penup, pendown, show, clear")
	fmt.Println()

	scanner := bufio.NewScanner(os.Stdin)
	var buffer strings.Builder

	prompt := "turtle> "
	fmt.Print(prompt)

	for scanner.Scan() {
		line := scanner.Text()

		if buffer.Len() > 0 {
			buffer.WriteString("\n")
		}
		buffer.WriteString(line)

		input := buffer.String()
		result := interp.Parse(input)

		switch result.Status {
		case feather.ParseIncomplete:
			// Need more input
			fmt.Print("   ...> ")
			continue

		case feather.ParseError:
			fmt.Println("Parse error:", result.Message)
			buffer.Reset()

		case feather.ParseOK:
			if strings.TrimSpace(input) != "" {
				res, err := interp.Eval(input)
				if err != nil {
					fmt.Println("Error:", err)
				} else if res.String() != "" {
					fmt.Println(res.String())
				}
			}
			buffer.Reset()
		}

		fmt.Print(prompt)
	}

	fmt.Println()
}
