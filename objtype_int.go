package feather

import "strconv"

// IntType is the internal representation for integer values.
type IntType int64

func (t IntType) Name() string         { return "int" }
func (t IntType) Dup() ObjType         { return t }
func (t IntType) UpdateString() string { return strconv.FormatInt(int64(t), 10) }

func (t IntType) IntoInt() (int64, bool)      { return int64(t), true }
func (t IntType) IntoDouble() (float64, bool) { return float64(t), true }
func (t IntType) IntoBool() (bool, bool)      { return t != 0, true }
