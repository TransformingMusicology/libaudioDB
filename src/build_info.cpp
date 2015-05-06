extern "C" {
#include "audioDB/audioDB_API.h"
}

const char* __lib_build_date()
{
#ifdef __LIB_BUILD_DATE
    return __LIB_BUILD_DATE;
#else
    return "unknown";
#endif
}

const char* __lib_build_number()
{
#ifdef __LIB_BUILD_NUMBER
    return __LIB_BUILD_NUMBER;
#else
    return "unknown";
#endif
}
