#if !defined(DV_INTRINSICS_H)
#ifndef DV_PLATFORM_H
#include "DV_Platform.h"
#endif
//
// TODO: Convert all of these to platform-efficient versions
//
#include <math.h>

inline real32 SquareRoot(real32 Real32)
{
    real32 Result = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(Real32)));
    return(Result);
}

inline int32 RoundReal32ToInt32(real32 Real32)
{
    int32 Result = _mm_cvtss_si32(_mm_set_ss(Real32));
    return(Result);
}

inline uint32 RoundReal32ToUInt32(real32 Real32)
{
    uint32 Result = (uint32)_mm_cvtss_si32(_mm_set_ss(Real32));
    return(Result);
}

#define DV_INTRINSICS_H
#endif
