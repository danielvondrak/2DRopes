#if !defined(DV_MATH_H)

#if !defined(DV_PLATFORM_H)
#include "DV_Platform.h"
#endif

typedef union v2
{
    struct
    {
        real32 X, Y;
    };
    real32 E[2];
} v2;

inline v2 V2(real32 X, real32 Y)
{
    v2 Result;

    Result.X = X;
    Result.Y = Y;

    return(Result);
}

inline v2 addV2(v2 A, v2 B)
{
	v2 Result = {A.X + B.X, A.Y + B.Y};
	return Result;
}

inline v2 scaleV2(v2 A, real32 B)
{
	v2 Result = {A.X*B, A.Y*B};
	return Result;
}

// inline v2 operator*(real32 A, v2 B)
// {
//     v2 Result;
//
//     Result.X = A*B.X;
//     Result.Y = A*B.Y;
//
//     return(Result);
// }
//
// inline v2
// operator*(v2 B, real32 A)
// {
//     v2 Result = A*B;
//
//     return(Result);
// }
//
// inline v2 &
// operator*=(v2 &B, real32 A)
// {
//     B = A * B;
//
//     return(B);
// }
//
// inline v2
// operator-(v2 A)
// {
//     v2 Result;
//
//     Result.X = -A.X;
//     Result.Y = -A.Y;
//
//     return(Result);
// }
//
// inline v2
// operator+(v2 A, v2 B)
// {
//     v2 Result;
//
//     Result.X = A.X + B.X;
//     Result.Y = A.Y + B.Y;
//
//     return(Result);
// }
//
// inline v2 &
// operator+=(v2 &A, v2 B)
// {
//     A = A + B;
//
//     return(A);
// }
//
// inline v2
// operator-(v2 A, v2 B)
// {
//     v2 Result;
//
//     Result.X = A.X - B.X;
//     Result.Y = A.Y - B.Y;
//
//     return(Result);
// }

inline real32 Square(real32 A)
{
	return A*A;
}

inline real32 Dot(v2 A, v2 B)
{
	return A.X*B.X + A.Y*B.Y;
}

inline real32 LengthSq(v2 A)
{
	return Dot(A, A);
}

#define DV_MATH_H
#endif
