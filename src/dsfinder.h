
struct asset_header
{
    font Font;
};

struct phased_clock
{
    u32 Stage;
    f32 t;
};

enum game_mode
{
    GameMode_FileExplorer,
    GameMode_DSFinder,
};

enum foot_type
{
    FootType_Jump = 0,
    FootType_Left,
    FootType_Right,
    FootType_PossiblyLeft,
    FootType_PossiblyRight,
    FootType_Unknown,
};

struct feet_state
{
    u32 LocationLeft;
    u32 LocationRight;
    b32 LeftIsGlued;
    b32 RightIsGlued;
    foot_type LastMoved;
};

struct doublestep
{
    u32 MeasureIndex;
    u32 MeasureNote;
    measure_text_data *Measure;
    foot_type MovingFoot;
    doublestep *Next;
    string String;
};

struct chart_doublestep_info
{
    string ChartInfo;
    doublestep *FirstDS;
    chart_doublestep_info *Next;
};

struct song_doublestep_info
{
    string Title;
    chart_doublestep_info *FirstChartInfo;
};

enum dsfinderline_type
{
    DSBusterLine_Title,
    DSBusterLine_ChartInfo,
    DSBusterLine_DS,
};

struct dsfinder_line
{
    string String;
    dsfinderline_type Type;
    doublestep *DS;
};

struct game_state
{
    b32 IsInitialized;
    memory_arena MainArena;
    asset_header *Assets;
    render_group RenderGroup;
    game_mode GameMode;
    
    phased_clock UpRepState;
    phased_clock DownRepState;
    
    string Path;
    filenames Filenames;
    temporary_memory FilenamesMemory;
    f32 ShowMessageEmptyDirClock;
    
    memory_arena FilenamesArena;
    u32 FilenameCursorIndex;
    u32 FilenameDrawTopIndex;
    
    b32 DSFinderIsInitialized;
    memory_arena SearchArena;
    temporary_memory DSFinderMemory;
    filenames AllFiles;
    
    song_doublestep_info *SongsDSInfo;
    u32 LinesCount;
    dsfinder_line *Lines;
    u32 DSCount;
    u32 TopLine;
    u32 CurrentLine;
    b32 ExitedEarly;
    f32 MeasuresCameraY;  // 0 iff centered on doublestep
    f32 LastClickY;
    f32 MeasuresCameraYLastClick;
    b32 GlueFeetOnHolds;
    b32 GlueFeetOnRolls;
    b32 TreatUUDDAsFootswitches;
};


struct note_draw_info
{
    rectangle2 Rect;
    v4 Color;
};
