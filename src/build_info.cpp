extern "C" {
#include "audioDB/audioDB_API.h"

const char* audiodb_lib_build_date()
{
#ifdef LIB_BUILD_DATE
    return LIB_BUILD_DATE;
#else
    return "unknown";
#endif
}

const char* audiodb_lib_build_number()
{
#ifdef LIB_BUILD_NUMBER
    return LIB_BUILD_NUMBER;
#else
    return "unknown";
#endif
}

}
