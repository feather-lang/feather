package harness

import (
	"io"
	"os"
	"strconv"
	"strings"

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

	// Find all test-case elements
	var findTestCases func(*html.Node)
	findTestCases = func(n *html.Node) {
		if n.Type == html.ElementNode && n.Data == "test-case" {
			tc := parseTestCase(n)
			suite.Cases = append(suite.Cases, tc)
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			findTestCases(c)
		}
	}
	findTestCases(doc)

	return suite, nil
}

// parseTestCase extracts a TestCase from a test-case HTML element.
func parseTestCase(n *html.Node) TestCase {
	tc := TestCase{}

	// Get name attribute
	for _, attr := range n.Attr {
		if attr.Key == "name" {
			tc.Name = attr.Val
			break
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
		case "result":
			tc.Result = strings.TrimSpace(content)
		case "error":
			tc.Error = strings.TrimSpace(content)
		case "stdout":
			tc.Stdout = normalizeLines(content)
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
