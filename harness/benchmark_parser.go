package harness

import (
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"golang.org/x/net/html"
)

// ParseBenchmarkFile parses a benchmark suite from the given file path.
func ParseBenchmarkFile(path string) (*BenchmarkSuite, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	suite, err := ParseBenchmark(f)
	if err != nil {
		return nil, err
	}
	suite.Path = path
	// Use filename without extension as suite name if not set
	if suite.Name == "" {
		base := filepath.Base(path)
		suite.Name = strings.TrimSuffix(base, filepath.Ext(base))
	}
	return suite, nil
}

// ParseBenchmark parses a benchmark suite from the given reader.
func ParseBenchmark(r io.Reader) (*BenchmarkSuite, error) {
	doc, err := html.Parse(r)
	if err != nil {
		return nil, err
	}

	suite := &BenchmarkSuite{
		Benchmarks: make([]Benchmark, 0),
	}

	// Find benchmark-suite element and benchmark elements
	var findElements func(*html.Node)
	findElements = func(n *html.Node) {
		if n.Type == html.ElementNode {
			switch n.Data {
			case "benchmark-suite":
				// Parse name attribute from benchmark-suite
				for _, attr := range n.Attr {
					if attr.Key == "name" {
						suite.Name = attr.Val
					}
				}
			case "benchmark":
				b := parseBenchmark(n)
				suite.Benchmarks = append(suite.Benchmarks, b)
			}
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			findElements(c)
		}
	}
	findElements(doc)

	return suite, nil
}

// parseBenchmark extracts a Benchmark from a benchmark HTML element.
func parseBenchmark(n *html.Node) Benchmark {
	b := Benchmark{
		Warmup:     0,
		Iterations: 1000, // Default to 1000 iterations
	}

	// Get attributes
	for _, attr := range n.Attr {
		switch attr.Key {
		case "name":
			b.Name = attr.Val
		case "warmup":
			if val, err := strconv.Atoi(attr.Val); err == nil {
				b.Warmup = val
			}
		case "iterations":
			if val, err := strconv.Atoi(attr.Val); err == nil {
				b.Iterations = val
			}
		}
	}

	// Extract child elements
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		if c.Type != html.ElementNode {
			continue
		}
		content := getTextContent(c)
		switch c.Data {
		case "setup":
			b.Setup = content
		case "script":
			b.Script = content
		}
	}

	return b
}
