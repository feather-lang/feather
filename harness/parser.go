package harness

import (
	"encoding/xml"
	"io"
	"os"
	"strconv"
	"strings"
)

// xmlTestSuite mirrors the XML structure for parsing.
type xmlTestSuite struct {
	XMLName   xml.Name      `xml:"test-suite"`
	TestCases []xmlTestCase `xml:"test-case"`
}

type xmlTestCase struct {
	Name     string `xml:"name,attr"`
	Script   string `xml:"script"`
	Result   string `xml:"result"`
	Error    string `xml:"error"`
	Stdout   string `xml:"stdout"`
	Stderr   string `xml:"stderr"`
	ExitCode string `xml:"exit-code"`
}

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
func Parse(r io.Reader) (*TestSuite, error) {
	var xs xmlTestSuite
	decoder := xml.NewDecoder(r)
	if err := decoder.Decode(&xs); err != nil {
		return nil, err
	}

	suite := &TestSuite{
		Cases: make([]TestCase, 0, len(xs.TestCases)),
	}

	for _, xtc := range xs.TestCases {
		exitCode := 0
		if xtc.ExitCode != "" {
			var err error
			exitCode, err = strconv.Atoi(strings.TrimSpace(xtc.ExitCode))
			if err != nil {
				return nil, err
			}
		}

		tc := TestCase{
			Name:     xtc.Name,
			Script:   strings.TrimSpace(xtc.Script),
			Result:   strings.TrimSpace(xtc.Result),
			Error:    strings.TrimSpace(xtc.Error),
			Stdout:   strings.TrimSpace(xtc.Stdout),
			Stderr:   strings.TrimSpace(xtc.Stderr),
			ExitCode: exitCode,
		}
		suite.Cases = append(suite.Cases, tc)
	}

	return suite, nil
}
