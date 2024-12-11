#ifndef _LPJS_MISC_H_
#define _LPJS_MISC_H_

enum
{
    LPJS_LOG_LEVEL_NORMAL,
    LPJS_LOG_LEVEL_DEBUG1,
    LPJS_LOG_LEVEL_DEBUG2
};

// For now.  Add --debug flag later.
#define lpjs_debug  lpjs_log

#include "misc-protos.h"

#endif

