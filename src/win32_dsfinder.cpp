#include <windows.h>
#include <intrin.h>
#include "common.h"
#include <gl/gl.h>
#include "dsfinder_opengl.cpp"
#include <xinput.h>
struct win32_backbuffer
{
    BITMAPINFO Info;
    void *Memory;
    s32 Width;
    s32 Height;
    s32 Pitch;
};

typedef BOOL WINAPI wgl_swap_interval_ext(int Interval);
global_variable wgl_swap_interval_ext *wglSwapInterval;

typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hShareContext, const int *AttribList);

global_variable win32_backbuffer GlobalBackbuffer;
global_variable b32 GlobalRunning;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};
global_variable LARGE_INTEGER GlobalCountsPerSecond;
global_variable game_input GlobalGameInput;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary)
        XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    if (!XInputLibrary)
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) 
            XInputGetState = XInputGetStateStub;
    }
}

PLATFORM_GET_INITIAL_PATH(Win32GetInitialPath)
{
    // TODO(vincent): figure this out
    u32 WrittenSize = GetCurrentDirectory(MaxSize-2, Path->Base);
    if (WrittenSize > MaxSize-3 || WrittenSize  < 3)
    {
        char *Dest = Path->Base;
        *Dest++ = 'C';
        *Dest++ = ':';
        *Dest++ = 0;
        Path->Size = 2;
    }
    else
    {
        char *At = Path->Base;
        while (*At)
            ++At;
        Path->Size = At - Path->Base;
    }
}

PLATFORM_APPEND_DIRNAME(Win32AppendDirname)
{
    b32 Appended = false;
    
    Assert(Path->Size >= 1);
    Assert(Path->Base[Path->Size-1] != '*');
    b32 HasEndingSlash = (Path->Base[Path->Size-1] == '\\');
    
    char *Source = Dirname.Base;
    char *Dest = Path->Base + Path->Size;
    char *End = Dest + Dirname.Size + (HasEndingSlash ? 0 : 1);
    
    if (End - Path->Base - 3 <= MaxSize)
    {
        Appended = true;
        if (!HasEndingSlash)
            *Dest++ = '\\';
        while (Dest < End)
            *Dest++ = *Source++;
        *Dest = 0;
        Path->Size = End - Path->Base;
    }
    return Appended;
}

PLATFORM_POP_DIRNAME(Win32PopDirname)
{
    b32 Popped = false;
    
    // TODO(vincent): maybe we could pop the drive and have PushFilenames list the drives in Windows
    // (It looks like FindFirstFile doesn't do that?)
    
    if (Path->Size > 2)
    {
        Assert(Path->Base[Path->Size-1] != '*');
        char *Cut = Path->Base + Path->Size - 1;
        while (Path->Base < Cut)
        {
            if (*Cut == '\\' || Cut == Path->Base + 1)
            {
                *Cut = 0;
                Path->Size = Cut - Path->Base;
                Popped = true;
                break;
            }
            --Cut;
        }
    }
    return Popped;
}

PLATFORM_PUSH_READ_FILE(Win32PushReadFile)
{
    string Result = {};
    HANDLE CreateFileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL, 0);
    if (CreateFileHandle != INVALID_HANDLE_VALUE)
    {
        Result.Size = GetFileSize(CreateFileHandle, 0);
        u32 AvailableSize = Arena->Size - Arena->Used;
        if (Result.Size <= AvailableSize)
        {
            Result.Base = PushArray(Arena, Result.Size, char);
            DWORD WrittenSize = 0;
            ReadFile(CreateFileHandle, Result.Base, Result.Size, &WrittenSize, 0);
            if (WrittenSize != Result.Size)
                Result.Base = 0;
        }
    }
    CloseHandle(CreateFileHandle);
    
    return Result;
}

PLATFORM_PUSH_FILENAMES(Win32PushFilenames)
{
    filenames Filenames = {};
    
    char *Suffix = Dirname;
    while (*Suffix)
        ++Suffix;
    char *SuffixAt = Suffix;
    *SuffixAt++ = '\\';
    *SuffixAt++ = '*';
    *SuffixAt = 0;
    
    WIN32_FIND_DATA FindFileData;
    
    HANDLE FindHandle = FindFirstFile(Dirname, &FindFileData);
    b32 IsDirectory = false;
    if (FindHandle != INVALID_HANDLE_VALUE)
    {
        IsDirectory = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        
        if ((IsDirectory && NeitherDotNorDotDot(FindFileData.cFileName)) || StringEndsWithCI(FindFileData.cFileName, ".sm"))
            ++Filenames.Count;
        while (FindNextFile(FindHandle, &FindFileData))
        {
            IsDirectory = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            if ((IsDirectory && FindFileData.cFileName[0] != '.') || StringEndsWithCI(FindFileData.cFileName, ".sm"))
                ++Filenames.Count;
        }
    }
    
    Filenames.Strings = PushArray(Arena, Filenames.Count, string);
    Filenames.IsDirectory = PushZeroArray(Arena, Filenames.Count, b32);
    Assert(!Filenames.Count || Filenames.IsDirectory[0] == 0);
    u32 FileIndex = 0;
    FindHandle = FindFirstFile(Dirname, &FindFileData);
    if (FindHandle != INVALID_HANDLE_VALUE)
    {
        IsDirectory = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        if ((IsDirectory && NeitherDotNorDotDot(FindFileData.cFileName)) ||
            StringEndsWithCI(FindFileData.cFileName, ".sm"))
        {
            Filenames.Strings[FileIndex] = PushString(Arena, FindFileData.cFileName);
            if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                Filenames.IsDirectory[FileIndex] = true;
            ++FileIndex;
        }
        while (FindNextFile(FindHandle, &FindFileData))
        {
            IsDirectory = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            if ((IsDirectory && NeitherDotNorDotDot(FindFileData.cFileName)) ||
                StringEndsWithCI(FindFileData.cFileName, ".sm"))
            {
                Filenames.Strings[FileIndex] = PushString(Arena, FindFileData.cFileName);
                if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    Filenames.IsDirectory[FileIndex] = true;
                }
                ++FileIndex;
            }
        }
    }
    FindClose(FindHandle);
    *Suffix = 0;
    return Filenames;
}

struct win32_game_code
{
#define DLL_FILENAME "dsfinder.dll"
#define DLL_COPY_FILENAME "dsfinder_copy.dll"
    game_update *Update;
    HMODULE DLL;
    FILETIME DLLWriteTime;
    WIN32_FILE_ATTRIBUTE_DATA DLLFileData;
};

internal void
Win32LoadGameCode(win32_game_code *Code)
{
#if DEBUG
    CopyFile(DLL_FILENAME, DLL_COPY_FILENAME, FALSE); // FALSE to overwrite
    Code->DLL = LoadLibraryA(DLL_COPY_FILENAME);
#else
    Code->DLL = LoadLibraryA(DLL_FILENAME);
#endif
    Code->Update = (game_update *)GetProcAddress(Code->DLL, "GameUpdate");
    Assert(Code->DLL && Code->Update);
    if (GetFileAttributesExA(DLL_FILENAME, GetFileExInfoStandard, &Code->DLLFileData))
        Code->DLLWriteTime = Code->DLLFileData.ftLastWriteTime;
}

internal void
ToggleFullScreen(HWND Window)
{
    // NOTE(vincent): Raymond Chen's blog article on fullscreen:
    // https://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx
    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {
            SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

global_variable GLuint GlobalBlitTextureHandle;
internal void
Win32DisplayBufferInWindow(game_render_commands *Commands, HWND Window,
                           game_memory *Memory)
{
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    HDC DeviceContext = GetDC(Window);
    s32 WindowWidth = ClientRect.right - ClientRect.left;
    s32 WindowHeight = ClientRect.bottom - ClientRect.top;
    b32 InHardware = true;
    if (InHardware)
    {
        OpenGLRenderToOutput(Commands, WindowWidth, WindowHeight);
        SwapBuffers(DeviceContext);
    }
    
    ReleaseDC(Window, DeviceContext);
    Commands->PushBufferSize = 0;
}

#include <Windowsx.h> // For GET_X_LPARAM()

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message)
    {
        case WM_MOUSEMOVE:
        {
            GlobalGameInput.MouseMoved = true;
            
            // NOTE(vincent): GetClientRect is not the same as GetWindowRect. 
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            
            u32 ClientWidth = ClientRect.right - ClientRect.left;
            u32 ClientHeight = ClientRect.bottom - ClientRect.top;
            
            s32 MouseClientX = GET_X_LPARAM(lParam);
            s32 MouseClientY = GET_Y_LPARAM(lParam);
            // NOTE(vincent): GET_X_LPARAM() returns some value in client space.
            // On my 1920*1080 monitor, in full screen mode, this maps to a range
            // of X values between 0 and 1919 inclusive.
            // Since the game is not aware about this client space, we want to remap
            // that to resolution space.
            
            // NOTE(vincent): Made these fire once, so I'm clamping.
            //Assert((s32)ClientWidth > MouseClientX);
            //Assert((s32)ClientHeight > MouseClientY);
            GlobalGameInput.MouseX = Clamp01(SafeRatio0((f32)MouseClientX, 
                                                        (f32)(ClientWidth-1)));
            GlobalGameInput.MouseY = Clamp01(1.0f - SafeRatio0((f32)MouseClientY, 
                                                               (f32)(ClientHeight-1)));
        } break;
        case WM_LBUTTONDOWN: GlobalGameInput.MouseLeft.IsPressed = true;  break;
        case WM_LBUTTONUP:   GlobalGameInput.MouseLeft.IsPressed = false; break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            u32 VKCode = (u32)wParam;
            
            b32 WasDown = ((lParam & (1 << 30)) != 0);
            b32 IsDown = ((lParam & (1 << 31)) == 0);
            b32 AltKeyWasDown = (lParam & (1 << 29));
            if (VKCode == VK_F4 && AltKeyWasDown) GlobalRunning = false;
            
            if (WasDown != IsDown)
            {
                switch (VKCode)
                {
#define SetPressed(KeyCode, Button) \
case KeyCode: GlobalGameInput.Button.IsPressed = IsDown; GlobalGameInput.Button.AutoRelease = false; break;
                    SetPressed(VK_LEFT,  Left);
                    SetPressed(VK_DOWN,  Down);
                    SetPressed(VK_UP,    Up);
                    SetPressed(VK_RIGHT, Right);
                    
                    SetPressed(VK_SPACE, A);
#undef SetPressed
                    case VK_F11: if(IsDown) ToggleFullScreen(Window); break;
                }
            }
        } break;
        
        case WM_QUIT: GlobalRunning = false; break;
        case WM_DESTROY: PostQuitMessage(0); GlobalRunning = false; break;
        case WM_ACTIVATEAPP: break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            BeginPaint(Window, &Paint);
            EndPaint(Window, &Paint);
        } break;
        
        default: return DefWindowProc(Window, Message, wParam, lParam);
    }
    return 0;
}

internal void
Win32PrepareBackbuffer(win32_backbuffer *Buffer, s32 Width, s32 Height)
{
    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->Pitch = Width*4;
    Buffer->Memory = VirtualAlloc(0, Width*Height*4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
}

internal LARGE_INTEGER
Win32GetWallClock()
{
    LARGE_INTEGER Timestamp;
    QueryPerformanceCounter(&Timestamp);
    return Timestamp;
}

internal f32
Win32GetSecondsElapsed(LARGE_INTEGER StartClock, LARGE_INTEGER EndClock)
{
    f32 Result = (f32)(EndClock.QuadPart - StartClock.QuadPart) / (f32)GlobalCountsPerSecond.QuadPart;
    return Result;
}

internal f32
Win32GetSecondsElapsedF64(LARGE_INTEGER StartClock, LARGE_INTEGER EndClock)
{
    f64 Result = (f64)(EndClock.QuadPart - StartClock.QuadPart) / (f64)GlobalCountsPerSecond.QuadPart;
    return Result;
}

internal void
Win32InitOpenGL(HWND Window)
{
    HDC WindowDC = GetDC(Window);
    PIXELFORMATDESCRIPTOR DesiredPixelFormat = {};
    DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
    DesiredPixelFormat.nVersion = 1;
    DesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    DesiredPixelFormat.cColorBits = 32;
    DesiredPixelFormat.cAlphaBits = 8;
    DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;
    DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
    
    int SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
    PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
    DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex, sizeof(SuggestedPixelFormat),
                        &SuggestedPixelFormat);
    SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);
    
    HGLRC OpenGLRC = wglCreateContext(WindowDC);
    if (wglMakeCurrent(WindowDC, OpenGLRC))
    {
        b32 ModernContext = false;
        // NOTE(vincent): Successfully set OpenGL context
        
        wgl_create_context_attribs_arb *wglCreateContextAttribsARB =
        (wgl_create_context_attribs_arb *)wglGetProcAddress("wglCreateContextAttribsARB");
        
        if (wglCreateContextAttribsARB)
        {
            // NOTE(vincent): This is a modern version of OpenGL
            
            int Attribs[] = 
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 0,
                WGL_CONTEXT_FLAGS_ARB, 0
#if DEBUG
                | WGL_CONTEXT_DEBUG_BIT_ARB
#endif
                ,
                // NOTE(vincent): Use compatibility profile bit arb in order to keep using the old fixed render functions pipeline: glTexCoord2f() glVertex2f() etc.
                // Modern OpenGL would otherwise require you to use shaders.
                // This is not guaranteed to succeed because implementations 
                // don't have to support the old things.
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                0, // termiate the attribs list with a zero like this.
            };
            
            HGLRC ShareContext = 0;
            HGLRC ModernGLRC = wglCreateContextAttribsARB(WindowDC, ShareContext, Attribs);
            if (ModernGLRC)
            {
                if (wglMakeCurrent(WindowDC, ModernGLRC))
                {
                    wglDeleteContext(OpenGLRC);
                    OpenGLRC = ModernGLRC;
                    ModernContext = true;
                }
            }
        }
        else
        {
            
        }
        
        //OpenGLInit(ModernContext);
        
        // NOTE(vincent): The proper way to load OpenGL extensions is to 
        // 1) check whether the OpenGL implementation has it with glGetString()
        // 2) use wglGetProcAddress() to load the function pointers we want
        
        wglSwapInterval = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
        if (wglSwapInterval)
            wglSwapInterval(1); // set VSync for every frame
    }
    else
        InvalidCodePath;
    
    ReleaseDC(Window, WindowDC);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    WNDCLASSA WindowClass = {0};
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;  // redraw window on resize
    
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = hInstance;
    WindowClass.hCursor = LoadCursorA(0, IDC_ARROW);
    WindowClass.lpszClassName = "WindowClassName";
    
    ATOM Atom = RegisterClassA(&WindowClass);
    Assert(Atom && "Couldn't register window class");
    
    HWND Window = CreateWindowExA(WS_EX_LEFT,
                                  WindowClass.lpszClassName, "dsfinder",
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT,   // initial X initial Y
                                  1920, 1080,                      // width height
                                  0, 0, hInstance, 0);
    
    Assert(Window && "Couldn't create the window");
    Win32InitOpenGL(Window);
    
    Win32PrepareBackbuffer(&GlobalBackbuffer, 1920, 1080);
    
    Win32LoadXInput();
    
    f32 GameUpdateHz = 60.0f;
    f32 TargetSecondsPerFrame = 1.0f / (f32)GameUpdateHz;
    GlobalGameInput.dtForFrame = TargetSecondsPerFrame;
    QueryPerformanceFrequency(&GlobalCountsPerSecond);
    LARGE_INTEGER InitialWallClock = Win32GetWallClock();
    LARGE_INTEGER LastWallClock = InitialWallClock;
    //u64 LastProcessorClock = __rdstc();
    UINT DesiredSchedulerMS = 1;
    b32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
    
    game_memory GameMemory = {};
    GameMemory.StorageSize = Gigabytes(1);
    LPVOID BaseAddress = (LPVOID) Terabytes(2);
    GameMemory.Storage = VirtualAlloc(BaseAddress, GameMemory.StorageSize,
                                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Assert(GameMemory.Storage);
    GameMemory.Platform.GetInitialPath = Win32GetInitialPath;
    GameMemory.Platform.PopDirname = Win32PopDirname;
    GameMemory.Platform.AppendDirname = Win32AppendDirname;
    GameMemory.Platform.PushReadFile = Win32PushReadFile;
    GameMemory.Platform.PushFilenames = Win32PushFilenames;
    win32_game_code GameCode = {};
    Win32LoadGameCode(&GameCode);
    
    u32 PushBufferSize = Megabytes(4);
    void *PushBuffer =
        VirtualAlloc(0, PushBufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    game_render_commands RenderCommands = 
        RenderCommandsStruct((u32)GlobalBackbuffer.Width, (u32)GlobalBackbuffer.Height,
                             PushBufferSize, PushBuffer);
    
    ShowWindow(Window, SW_SHOWDEFAULT);
    
    GlobalRunning = true;
    while (GlobalRunning)
    {
        for (int i = 0; i < ArrayCount(GlobalGameInput.AllButtons); ++i)
        {
            GlobalGameInput.AllButtons[i].WasPressed = GlobalGameInput.AllButtons[i].IsPressed;
            if (GlobalGameInput.AllButtons[i].AutoRelease)
                GlobalGameInput.AllButtons[i].IsPressed = false;
        }
        GlobalGameInput.MouseMoved = false;
        
        MSG Message;
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
        
        // NOTE(vincent): XInput
        XINPUT_STATE ControllerState;
        if (XInputGetState(0, &ControllerState) == ERROR_SUCCESS)
        {
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
            
            f32 NormalizedLX = (Pad->sThumbLX + .5f) / 32767.5f;
            f32 NormalizedLY = (Pad->sThumbLY + .5f) / 32767.5f;
            
            f32 Magnitude = SquareRoot(Square(NormalizedLX) + Square(NormalizedLY));
            f32 Deadzone = .25f;
            f32 ProcessedMagnitude = 0.0f;
            if (Magnitude > Deadzone)
                ProcessedMagnitude = (Magnitude - Deadzone) / (1.0f - Deadzone);
            f32 MagnitudeRatio = SafeRatio0(ProcessedMagnitude, Magnitude);
            GlobalGameInput.ThumbLX = MagnitudeRatio * NormalizedLX;
            GlobalGameInput.ThumbLY = MagnitudeRatio * NormalizedLY;
            
            f32 NormalizedRX = (Pad->sThumbRX + .5f) / 32767.5f;
            f32 NormalizedRY = (Pad->sThumbRY + .5f) / 32767.5f;
            f32 MagnitudeR = SquareRoot(Square(NormalizedRX) + Square(NormalizedRY));
            f32 ProcessedMagnitudeR = 0.0f;
            if (MagnitudeR > Deadzone)
                ProcessedMagnitudeR = (MagnitudeR - Deadzone) / (1.0f - Deadzone);
            f32 MagnitudeRatioR = SafeRatio0(ProcessedMagnitudeR, MagnitudeR);
            GlobalGameInput.ThumbRX = MagnitudeRatioR * NormalizedRX;
            GlobalGameInput.ThumbRY = MagnitudeRatioR * NormalizedRY;
            
            if (!GlobalGameInput.Left.IsPressed)
            {
                GlobalGameInput.Left.IsPressed = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                GlobalGameInput.Left.AutoRelease = true;
            }
            if (!GlobalGameInput.Down.IsPressed)
            {
                GlobalGameInput.Down.IsPressed = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                GlobalGameInput.Down.AutoRelease = true;
            }
            if (!GlobalGameInput.Up.IsPressed)
            {
                GlobalGameInput.Up.IsPressed = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                GlobalGameInput.Up.AutoRelease = true;
            }
            if (!GlobalGameInput.Right.IsPressed)
            {
                GlobalGameInput.Right.IsPressed = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                GlobalGameInput.Right.AutoRelease = true;
            }
            if (!GlobalGameInput.A.IsPressed)
            {
                GlobalGameInput.A.IsPressed = (Pad->wButtons & XINPUT_GAMEPAD_A);
                GlobalGameInput.A.AutoRelease = true;
            }
            if (!GlobalGameInput.B.IsPressed)
            {
                GlobalGameInput.B.IsPressed = (Pad->wButtons & XINPUT_GAMEPAD_B);
                GlobalGameInput.B.AutoRelease = true;
            }
            if (!GlobalGameInput.X.IsPressed)
            {
                GlobalGameInput.X.IsPressed = (Pad->wButtons & XINPUT_GAMEPAD_X);
                GlobalGameInput.X.AutoRelease = true;
            }
            if (!GlobalGameInput.Y.IsPressed)
            {
                GlobalGameInput.Y.IsPressed = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                GlobalGameInput.Y.AutoRelease = true;
            }
        }
        
#if DEBUG
        // NOTE(vincent): Check DLL write time and reload it if it's new
        GetFileAttributesExA(DLL_FILENAME, GetFileExInfoStandard, &GameCode.DLLFileData);
        GlobalGameInput.ReloadedDLL = 0;
        if (CompareFileTime(&GameCode.DLLFileData.ftLastWriteTime, &GameCode.DLLWriteTime))
        {
            FreeLibrary(GameCode.DLL);
            Win32LoadGameCode(&GameCode);
            GlobalGameInput.ReloadedDLL = 1;
        }
#endif
        
        GameCode.Update(&GameMemory, &GlobalGameInput, &RenderCommands);
        
        Win32DisplayBufferInWindow(&RenderCommands, Window, &GameMemory);
        RenderCommands.PushBufferSize = 0;
        
        // NOTE(vincent): wait for frame time to finish
        LARGE_INTEGER WorkCounter = Win32GetWallClock();
        f32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastWallClock, WorkCounter);
        f32 SecondsElapsedForFrame = WorkSecondsElapsed;
        if (SecondsElapsedForFrame < TargetSecondsPerFrame)
        {
            if (SleepIsGranular)
            {
                DWORD SleepMS = (DWORD) (1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                if (SleepMS > 0)
                    Sleep(SleepMS);
            }
            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastWallClock, Win32GetWallClock());
            
            while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastWallClock, Win32GetWallClock());
        }
        LastWallClock = Win32GetWallClock(); // TODO(vincent): less dumb update
    }
}