package harness

import (
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"golang.org/x/net/html"
)

// ParseFile parses a test suite from the given file path.
func ParseFile(path string) (*TestSuite, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	suite, err := Parse(f)
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

// Parse parses a test suite from the given reader.
// Uses HTML parser so that <script> content is treated as raw text
// without requiring CDATA sections.
func Parse(r io.Reader) (*TestSuite, error) {
	doc, err := html.Parse(r)
	if err != nil {
		return nil, err
	}

	suite := &TestSuite{
		Cases: make([]TestCase, 0),
	}

	// Find test-suite element and test-case elements
	var findElements func(*html.Node)
	findElements = func(n *html.Node) {
		if n.Type == html.ElementNode {
			switch n.Data {
			case "test-suite":
				// Parse timeout attribute from test-suite
				for _, attr := range n.Attr {
					if attr.Key == "timeout" {
						if d, err := time.ParseDuration(attr.Val); err == nil {
							suite.Timeout = d
						}
					} else if attr.Key == "name" {
						suite.Name = attr.Val
					}
				}
			case "test-case":
				tc := parseTestCase(n)
				suite.Cases = append(suite.Cases, tc)
			}
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			findElements(c)
		}
	}
	findElements(doc)

	return suite, nil
}

// parseTestCase extracts a TestCase from a test-case HTML element.
func parseTestCase(n *html.Node) TestCase {
	tc := TestCase{}

	// Get attributes
	for _, attr := range n.Attr {
		switch attr.Key {
		case "name":
			tc.Name = attr.Val
		case "timeout":
			if d, err := time.ParseDuration(attr.Val); err == nil {
				tc.Timeout = d
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
		case "script":
			tc.Script = content
		case "return":
			tc.Return = strings.TrimSpace(content)
		case "result":
			tc.Result = strings.TrimSpace(content)
		case "error":
			tc.Error = strings.TrimSpace(content)
		case "stdout":
			tc.Stdout = normalizeLines(content)
			tc.StdoutSet = true
		case "stderr":
			tc.Stderr = normalizeLines(content)
		case "exit-code":
			exitCode, _ := strconv.Atoi(strings.TrimSpace(content))
			tc.ExitCode = exitCode
		}
	}

	return tc
}

// getTextContent returns all text content within an element.
func getTextContent(n *html.Node) string {
	var sb strings.Builder
	var collect func(*html.Node)
	collect = func(node *html.Node) {
		if node.Type == html.TextNode {
			sb.WriteString(node.Data)
		}
		for c := node.FirstChild; c != nil; c = c.NextSibling {
			collect(c)
		}
	}
	collect(n)
	return sb.String()
}

// normalizeLines treats content as a list of lines, trims each line,
// and removes only leading and trailing empty lines.
func normalizeLines(content string) string {
	lines := strings.Split(content, "\n")
	// Trim each line
	for i, line := range lines {
		lines[i] = strings.TrimSpace(line)
	}
	// Skip leading empty lines
	start := 0
	for start < len(lines) && lines[start] == "" {
		start++
	}
	// Skip trailing empty lines
	end := len(lines)
	for end > start && lines[end-1] == "" {
		end--
	}
	if start >= end {
		return ""
	}
	return strings.Join(lines[start:end], "\n")
}
