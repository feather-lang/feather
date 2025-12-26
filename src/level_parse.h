#ifndef FEATHER_LEVEL_PARSE_H
#define FEATHER_LEVEL_PARSE_H

#include "feather.h"

FeatherResult feather_parse_level(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj levelObj, size_t currentLevel,
                                  size_t stackSize, size_t *absLevel);

#endif
