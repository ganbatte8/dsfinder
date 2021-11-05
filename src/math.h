
struct v2
{
    f32 x;
    f32 y;
};

union v3
{
    struct
    {
        f32 x;
        f32 y;
        f32 z;
    };
    struct
    {
        f32 r;
        f32 g;
        f32 b;
    };
};

union v4
{
    struct 
    {
        union
        {
            struct
            {
                f32 x;
                f32 y;
                f32 z;
            };
            v3 xyz;
        };
        f32 w;
    };
    
    struct
    {
        union
        {
            struct
            {
                f32 r;
                f32 g;
                f32 b;
            };
            v3 rgb;
        };
        f32 a;
    };
};

internal v2
V2(f32 x, f32 y)
{
    v2 Result = {x, y};
    return Result;
}

internal v2
V2i(s32 x, s32 y)
{
    v2 Result = {(f32)x, (f32)y};
    return Result;
}

internal v3
V3(f32 x, f32 y, f32 z)
{
    v3 Result = {x, y, z};
    return Result;
}

internal v4
V4(f32 x, f32 y, f32 z, f32 w)
{
    v4 Result = {x, y, z, w};
    return Result;
}

struct rectangle2
{
    v2 Min;
    v2 Max;
};

struct rectangle2i
{
    s32 MinX;
    s32 MinY;
    s32 MaxX;
    s32 MaxY;
};

inline v2
operator+(v2 A, v2 B)
{
    v2 Result = {A.x + B.x, A.y + B.y};
    return Result;
}

inline v3
operator+(v3 A, v3 B)
{
    v3 Result = {A.x + B.x, A.y + B.y, A.z + B.z};
    return Result;
}

inline v2 &
operator+=(v2 &A, v2 B)
{
    A = A + B;
    return A;
}

inline v2
operator-(v2 A, v2 B)
{
    v2 Result = {A.x - B.x, A.y - B.y};
    return Result;
}

inline v2
operator-(v2 A)
{
    v2 Result = {-A.x, -A.y};
    return Result;
}

inline v2 &
operator-=(v2 &A, v2 B)
{
    A = A - B;
    return A;
}

inline v2
operator*(f32 Scalar, v2 V)
{
    v2 Result = {V.x * Scalar, V.y * Scalar};
    return Result;
}

inline v2
operator*(v2 V, f32 Scalar)
{
    v2 Result = {V.x * Scalar, V.y * Scalar};
    return Result;
}

inline v2 &
operator*=(v2 &V, f32 Scalar)
{
    V = Scalar * V;
    return V;
}

inline v3
operator*(v3 V, f32 Scalar)
{
    v3 Result;
    Result.x = V.x * Scalar;
    Result.y = V.y * Scalar;
    Result.z = V.z * Scalar;
    return Result;
}

inline v3
operator*(f32 Scalar, v3 V)
{
    v3 Result;
    Result.x = Scalar * V.x;
    Result.y = Scalar * V.y;
    Result.z = Scalar * V.z;
    return Result;
}

inline v3 &
operator*=(v3 &V, f32 Scalar)
{
    V = Scalar * V;
    return V;
}

inline v4
operator+(v4 A, v4 B)
{
    v4 Result;
    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;
    Result.w = A.w + B.w;
    return Result;
}

inline v4 &
operator+=(v4 &A, v4 B)
{
    A = A + B;
    return A;
}

inline v4
operator-(v4 A, v4 B)
{
    v4 Result;
    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;
    Result.w = A.w - B.w;
    return Result;
}


inline v4 &
operator-=(v4 &A, v4 B)
{
    A = A - B;
    return A;
}

inline v4
operator*(f32 Scalar, v4 V)
{
    v4 Result;
    Result.x = V.x * Scalar;
    Result.y = V.y * Scalar;
    Result.z = V.z * Scalar;
    Result.w = V.w * Scalar;
    return Result;
}

inline v4
operator*(v4 V, f32 Scalar)
{
    v4 Result;
    Result.x = V.x * Scalar;
    Result.y = V.y * Scalar;
    Result.z = V.z * Scalar;
    Result.w = V.w * Scalar;
    return Result;
}

inline v4 &
operator*=(v4 &V, f32 Scalar)
{
    V = Scalar * V;
    return V;
}

internal rectangle2
RectCenterDim(v2 Center, v2 Dim)
{
    rectangle2 Result;
    v2 HalfDim = Dim * 0.5f;
    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;
    return Result;
}

internal rectangle2
RectMinMax(v2 Min, v2 Max)
{
    rectangle2 Rect = {Min, Max};
    return Rect;
}

internal b32
IsInRectangle(rectangle2 Rect, v2 P)
{
    b32 Result = (Rect.Min.x <= P.x && P.x < Rect.Max.x &&
                  Rect.Min.y <= P.y && P.y < Rect.Max.y);
    return Result;
}

internal f32
Lerp(f32 A, f32 t, f32 B)
{
    f32 Result = (1.0f - t) * A + t*B;
    return Result;
}

internal v2
Lerp(v2 A, f32 t, v2 B)
{
    v2 Result = (1.0f - t) * A + t*B;
    return Result;
}
internal v4
Lerp(v4 A, f32 t, v4 B)
{
    v4 Result = (1.0f - t) * A + t*B;
    return Result;
}

internal f32
SafeRatio1(f32 Dividend, f32 Divisor)
{
    if (Divisor == 0.0f)
        return 1.0f;
    return Dividend / Divisor;
}

internal f32
SafeRatio0(f32 Dividend, f32 Divisor)
{
    if (Divisor == 0.0f)
        return 0.0f;
    return Dividend / Divisor;
}

#define Minimum(A,B) ((A) < (B) ? (A) : (B))
#define Maximum(A,B) ((A) < (B) ? (B) : (A))
#define AbsoluteValue(A) Maximum(A, 0)

internal f32
Clamp01(f32 Value)
{
    f32 Result = Value;
    if (Result < 0.0f)
        Result = 0.0f;
    else if (Result > 1.0f)
        Result = 1.0f;
    return Result;
}

internal rectangle2
InvertedInfinityRectangle(void)
{
    rectangle2 Result;
    // TODO(vincent): assign actual float max values?
    Result.Min.x = 1.0f;
    Result.Max.x = -1.0f;
    Result.Min.y = 1.0f;
    Result.Max.y = -1.0f;
    return Result;
}

internal rectangle2
RectUnion(rectangle2 A, rectangle2 B)
{
    rectangle2 Result;
    Result.Min.x = A.Min.x < B.Min.x ? A.Min.x : B.Min.x;
    Result.Min.y = A.Min.y < B.Min.y ? A.Min.y : B.Min.y;
    Result.Max.x = A.Max.x > B.Max.x ? A.Max.x : B.Max.x;
    Result.Max.y = A.Max.y > B.Max.y ? A.Max.y : B.Max.y;
    return Result;
}

internal f32
Square(f32 t)
{
    f32 Result = t*t;
    return Result;
}

internal f32
SquareRoot(f32 t)
{
    // TODO(vincent): Prefer hardware instruction instead of full software Newton-Raphson
    Assert(t >= 0.0f);
    f32 x = t;
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    x = 0.5f*(x + SafeRatio0(t, x));
    
#if DEBUG
    Assert(x >= 0.0f);
    f32 AbsError = AbsoluteValue(x*x - t);
    f32 RelError = SafeRatio0(AbsError, t);
    Assert(RelError <= 0.001f || AbsError <= .001f);
#endif
    return x;
}


internal v2
Hadamard(v2 A, v2 B)
{
    v2 Result = {A.x * B.x, A.y * B.y};
    return Result;
}

