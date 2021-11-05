#include <stdint.h>

#if COMPILER_MSVC
#define CompilerWriteBarrier _WriteBarrier();
#else
#define CompilerWriteBarrier asm volatile("" ::: "memory")
#endif

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int b32;
typedef float f32;
typedef double f64;

#define global_variable static
#define internal static
#define Kilobytes(Value) ((Value) * 1000LL)
#define Megabytes(Value) (Kilobytes(Value) * 1000LL)
#define Gigabytes(Value) (Megabytes(Value) * 1000LL)
#define Terabytes(Value) (Gigabytes(Value) * 1000LL)

#if DEBUG
#define InvalidCodePath {*(int*)0 = 0;}
#define Assert(Expression) if (!(Expression)) InvalidCodePath;
#define InvalidDefaultCase default: InvalidCodePath;
#else
#define Assert(Expression)
#define InvalidCodePath
#define InvalidDefaultCase default: {}
#endif

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define Align8(Count)  (((Count) + 7) & ~7)
#define Align2(Count)  (((Count) + 1) & ~1)

struct memory_arena
{
    u32 Size;
    u8 *Base;
    u32 Used;
    u32 TempCount;
};

inline void
InitializeArena(memory_arena *Arena, u32 Size, void *Base)
{
    Arena->Size = Size;
    Arena->Base = (u8 *)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

inline void *
PushSize(memory_arena *Arena, u32 Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}

inline void *
PushZeroSize(memory_arena *Arena, u32 Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    char *C = (char*)Result;
    for (u32 i = 0; i < Size; ++i)
        *C++ = 0;
    Arena->Used += Size;
    return Result;
}

#define PushStruct(Arena, type) (type *)PushSize(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize(Arena, (Count)*sizeof(type))
#define PushZeroStruct(Arena, type) (type *)PushZeroSize(Arena, sizeof(type))
#define PushZeroArray(Arena, Count, type) (type *)PushZeroSize(Arena, (Count)*sizeof(type))
#define PushFakeStruct(Arena, type) (type *)PushSize(Arena, 0)

struct temporary_memory
{
    memory_arena *Arena;
    u32 Used;
};

inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;
    Result.Arena = Arena;
    Result.Used = Arena->Used;
    ++Arena->TempCount;
    return Result;
}

inline void
EndTemporaryMemory(temporary_memory Temp)
{
    Assert(Temp.Arena->Used >= Temp.Used);
    Temp.Arena->Used = Temp.Used;
    Assert(Temp.Arena->TempCount > 0);
    Temp.Arena->TempCount--;
}

inline void
CommitTemporaryMemory(temporary_memory Temp)
{
    Assert(Temp.Arena->Used >= Temp.Used);
    Assert(Temp.Arena->TempCount > 0);
    Temp.Arena->TempCount--;
}

inline void
CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

internal void
SubArena(memory_arena *Result, memory_arena *Arena, u32 Size)
{
    Result->Size = Size;
    Result->Base = (u8 *)PushSize(Arena, Size);
    Result->Used = 0;
    Result->TempCount = 0;
}

#define BYTES_PER_PIXEL 4
#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_GCC
#include <x86intrin.h>
//#include <xmmintrin.h>
#endif

struct game_backbuffer
{
    void *Memory;
    s32 Width;
    s32 Height;
    s32 Pitch;
};

struct game_render_commands
{
    u32 Width;
    u32 Height;
    
    // TODO(vincent): memory arena here?
    u32 MaxPushBufferSize;
    u32 PushBufferSize;
    u8 *PushBufferBase;
};

#define RenderCommandsStruct(Width, Height, MaxPushBufferSize, PushBuffer) \
{Width, Height, MaxPushBufferSize, 0, (u8 *)PushBuffer};


#define PLATFORM_WRITE_FILE(name) b32 name(char *Filename, u32 Size, void *Memory)
typedef PLATFORM_WRITE_FILE(platform_write_file);

struct string
{
    char *Base;
    u32 Size;
};

internal string
PushString(memory_arena *Arena, char *CString)
{
    u32 SizeWithNull = 1;
    for (char *Source = CString; *Source; ++Source)
        ++SizeWithNull;
    
    char *Dest = PushArray(Arena, SizeWithNull, char);
    string String;
    String.Base = Dest;
    String.Size = SizeWithNull - 1;
    
    while (*CString)
        *Dest++ = *CString++;
    *Dest = 0;
    return String;
}

internal b32
StringEquals(char *A, char *B)
{
    b32 Result = true;
    while (*A && *B)
    {
        if (*A != *B)
        {
            Result = false;
            break;
        }
        ++A;
        ++B;
    }
    Result = Result && (*A == *B);
    return Result;
}

internal b32
StringEquals(string A, char *B)
{
    b32 Result = true;
    char *SourceA = A.Base;
    for (u32 i = 0; i < A.Size; ++i)
    {
        if (*SourceA != *B)
        {
            Result = false;
            break;
        }
        ++SourceA;
        ++B;
    }
    if (Result)
        Result = (*B == 0);
    return Result;
}

internal b32
StringEquals(string A, string B)
{
    b32 Result = true;
    if (A.Size != B.Size)
        Result = false;
    else
    {
        char *Source1 = A.Base;
        char *Source2 = B.Base;
        char *SourceEnd = Source1+A.Size;
        while (Source1 < SourceEnd)
        {
            if (*Source1 != *Source2)
            {
                Result = false;
                break;
            }
            ++Source1;
            ++Source2;
        }
    }
    return Result;
}

internal b32
NeitherDotNorDotDot(char *S)
{
    if (*S == '.')
    {
        ++S;
        if (*S == 0)
            return false;
        if (*S == '.')
        {
            ++S;
            if (*S == 0)
                return false;
        }
    }
    return true;
}

internal b32
StringEndsWithCI(char *Source, char *Pattern)
{
    b32 Result = false;
    char *At = Source;
    while (*At)
        ++At;
    u32 SourceLength = At - Source;
    char *P = Pattern;
    while (*P)
        ++P;
    u32 PatternLength = P - Pattern;
    if (PatternLength <= SourceLength)
    {
        Result = true;
        for (u32 i = 0; i < PatternLength; ++i)
        {
            --P;
            --At;
            if ((*P | 32) != (*At | 32))
            {
                Result = false;
                break;
            }
        }
    }
    
    return Result;
}

struct filenames
{
    u32 Count;
    string *Strings;
    b32 *IsDirectory;
};

#define PLATFORM_PUSH_READ_FILE(name) string name(memory_arena *Arena, char *Filename)
typedef PLATFORM_PUSH_READ_FILE(platform_push_read_file);

#define PLATFORM_PUSH_FILENAMES(name) filenames name(memory_arena *Arena, char *Dirname)
typedef PLATFORM_PUSH_FILENAMES(platform_push_filenames);
#define PLATFORM_GET_INITIAL_PATH(name) void name(string *Path, u32 MaxSize)
typedef PLATFORM_GET_INITIAL_PATH(platform_get_initial_path);

#define PLATFORM_APPEND_DIRNAME(name) b32 name(string *Path, string Dirname, u32 MaxSize)  
typedef PLATFORM_APPEND_DIRNAME(platform_append_dirname);
#define PLATFORM_POP_DIRNAME(name) b32 name(string *Path)
typedef PLATFORM_POP_DIRNAME(platform_pop_dirname);

struct platform_api
{
    platform_push_read_file *PushReadFile;
    platform_push_filenames *PushFilenames;
    platform_get_initial_path *GetInitialPath;
    platform_append_dirname *AppendDirname;
    platform_pop_dirname *PopDirname;
};

struct game_memory
{
    void *Storage;
    u32 StorageSize;
    platform_api Platform;
};

struct button_state
{
    b32 IsPressed;
    b32 WasPressed;
    b32 AutoRelease;
};

struct game_input
{
    union
    {
        struct
        {
            button_state A; // space, launch/quit parser 
            button_state B;
            button_state X;
            button_state Y;
            
            // arrow keys / Dpad: 
            button_state Left; 
            button_state Down;
            button_state Up;
            button_state Right;
            
            button_state MouseLeft;
        };
        button_state AllButtons[9];
    };
    
    f32 MouseX;   // [0.0f, 1.0f]. Left to right
    f32 MouseY;   // Bottom to top
    b32 MouseMoved;
    
    f32 ThumbLX;  // [-1.0f, 1.0f]. Left to right.
    f32 ThumbLY;  // Bottom to top
    f32 ThumbRX;
    f32 ThumbRY;
    
    f32 dtForFrame;
    b32 ReloadedDLL;
};

#define GAME_UPDATE(name) void name(game_memory *Memory, game_input *Input, game_render_commands *RenderCommands)
typedef GAME_UPDATE(game_update);


internal b32
IsWhitespace(char C)
{
    b32 Result = false;
    if (C == ' ' || C == '\t' || C == '\v' || C == '\f' || C == '\n' || C == '\r')
        Result = true;
    return Result;
}

#include "math.h"
#include "render_entries.h"