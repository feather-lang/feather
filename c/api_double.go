// api_double.go provides double (floating-point) operations for the low-level C API.
package main

/*
#include <stdint.h>
*/
import "C"

import (
	"math"
	"strconv"
	"strings"
)

// Double classification constants (matching feather_api.h)
const (
	featherDblNormal    = 0
	featherDblSubnormal = 1
	featherDblZero      = 2
	featherDblInf       = 3
	featherDblNegInf    = 4
	featherDblNan       = 5
)

// Math operation constants (matching feather_api.h)
const (
	mathSqrt  = 0
	mathExp   = 1
	mathLog   = 2
	mathLog10 = 3
	mathSin   = 4
	mathCos   = 5
	mathTan   = 6
	mathAsin  = 7
	mathAcos  = 8
	mathAtan  = 9
	mathSinh  = 10
	mathCosh  = 11
	mathTanh  = 12
	mathFloor = 13
	mathCeil  = 14
	mathRound = 15
	mathAbs   = 16
	mathPow   = 17
	mathAtan2 = 18
	mathFmod  = 19
	mathHypot = 20
)

// -----------------------------------------------------------------------------
// Double Operations
// -----------------------------------------------------------------------------

//export feather_double_create
func feather_double_create(interp C.size_t, val C.double) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	obj := i.Double(float64(val))
	return C.size_t(reg.registerObj(obj))
}

//export feather_double_get
func feather_double_get(interp C.size_t, obj C.size_t, out *C.double) C.int {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 1 // FEATHER_ERROR
	}
	o := reg.getObj(uintptr(obj))
	if o == nil {
		return 1
	}
	val, err := o.Double()
	if err != nil {
		return 1
	}
	*out = C.double(val)
	return 0 // FEATHER_OK
}

//export feather_double_classify
func feather_double_classify(val C.double) C.int {
	v := float64(val)
	if math.IsNaN(v) {
		return featherDblNan
	}
	if math.IsInf(v, 1) {
		return featherDblInf
	}
	if math.IsInf(v, -1) {
		return featherDblNegInf
	}
	if v == 0 {
		return featherDblZero
	}
	// Smallest normal positive float64 is 2^-1022
	const smallestNormal = 2.2250738585072014e-308
	absV := math.Abs(v)
	if absV < smallestNormal {
		return featherDblSubnormal
	}
	return featherDblNormal
}

//export feather_double_format
func feather_double_format(interp C.size_t, val C.double, spec C.char, precision C.int, alt C.int) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	v := float64(val)
	prec := int(precision)
	useAlt := int(alt) != 0

	// Handle special values
	var result string
	if math.IsNaN(v) {
		result = "NaN"
	} else if math.IsInf(v, 1) {
		result = "Inf"
	} else if math.IsInf(v, -1) {
		result = "-Inf"
	} else {
		// Default precision
		if prec < 0 {
			prec = 6
		}

		// Format based on specifier
		var format byte
		switch byte(spec) {
		case 'e', 'E':
			format = byte(spec)
		case 'f', 'F':
			format = 'f'
		case 'g', 'G':
			format = byte(spec)
		default:
			format = 'g'
		}

		result = strconv.FormatFloat(v, format, prec, 64)

		// Handle alternate form (#)
		if useAlt && !strings.Contains(result, ".") {
			if !strings.Contains(result, "e") && !strings.Contains(result, "E") {
				result = result + "."
			}
		}
	}

	obj := i.String(result)
	return C.size_t(reg.registerObj(obj))
}

//export feather_double_math
func feather_double_math(interp C.size_t, op C.int, a C.double, b C.double, out *C.double) C.int {
	va := float64(a)
	vb := float64(b)

	var result float64
	switch int(op) {
	case mathSqrt:
		result = math.Sqrt(va)
	case mathExp:
		result = math.Exp(va)
	case mathLog:
		result = math.Log(va)
	case mathLog10:
		result = math.Log10(va)
	case mathSin:
		result = math.Sin(va)
	case mathCos:
		result = math.Cos(va)
	case mathTan:
		result = math.Tan(va)
	case mathAsin:
		result = math.Asin(va)
	case mathAcos:
		result = math.Acos(va)
	case mathAtan:
		result = math.Atan(va)
	case mathSinh:
		result = math.Sinh(va)
	case mathCosh:
		result = math.Cosh(va)
	case mathTanh:
		result = math.Tanh(va)
	case mathFloor:
		result = math.Floor(va)
	case mathCeil:
		result = math.Ceil(va)
	case mathRound:
		result = math.Round(va)
	case mathAbs:
		result = math.Abs(va)
	case mathPow:
		result = math.Pow(va, vb)
	case mathAtan2:
		result = math.Atan2(va, vb)
	case mathFmod:
		result = math.Mod(va, vb)
	case mathHypot:
		result = math.Hypot(va, vb)
	default:
		return 1 // FEATHER_ERROR
	}

	*out = C.double(result)
	return 0 // FEATHER_OK
}
