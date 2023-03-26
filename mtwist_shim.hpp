#pragma once

// this is so stupid

extern "C" {
#ifdef __cplusplus
#   undef __cplusplus
#   include "mtwist.h"
#   define __cplusplus
#else
#   include "mtwist.h"
#endif
}
