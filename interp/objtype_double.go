package interp

import (
	"strconv"
	"strings"
)

// DoubleType is the internal representation for floating-point values.
type DoubleType float64

func (t DoubleType) Name() string { return "double" }
func (t DoubleType) Dup() ObjType { return t }
func (t DoubleType) UpdateString() string {
	s := strconv.FormatFloat(float64(t), 'g', -1, 64)
	// Go uses +Inf/-Inf, but TCL uses Inf/-Inf
	if s == "+Inf" {
		s = "Inf"
	}
	// Add .0 for round numbers, but not for NaN/Inf
	if !strings.Contains(s, ".") && !strings.Contains(s, "e") &&
		!strings.Contains(s, "NaN") && !strings.Contains(s, "Inf") {
		s += ".0"
	}
	return s
}

func (t DoubleType) IntoInt() (int64, bool)      { return int64(t), true }
func (t DoubleType) IntoDouble() (float64, bool) { return float64(t), true }
