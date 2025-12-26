#ifndef FEATHER_INDEX_PARSE_H
#define FEATHER_INDEX_PARSE_H

#include "feather.h"

FeatherResult feather_parse_index(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj indexObj, size_t listLen, int64_t *out);

#endif
