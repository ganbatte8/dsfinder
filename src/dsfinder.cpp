// TODO(vincent):
// - Windows layer
// - strip away code when finished
// - will the released program produce some crappy dll even if reloading is not used?
#include "common.h"
#include "dsfinder_render_group.cpp"
global_variable platform_api *GlobalPlatform;
#include "dsfinder_parser.cpp"
#include "dsfinder.h"

#define PATH_BUFFER_SIZE Kilobytes(20)

internal u32
WriteString(char *Dest, const char *Source)
{
    // NOTE(vincent): assume Source is null-terminated, but don't null-terminate Dest.
    char *D = Dest;
    while (*Source)
        *D++ = *Source++;
    u32 Count = D - Dest;
    return Count;
}

internal u32
WriteNumber(char *Dest, u32 Number)
{
    char Buffer[10] = {0};
    char *P = Buffer;
    if (Number == 0)
        *P++ = Number;
    while (Number > 0)
    {
        *P++ = Number % 10;
        Number /= 10;
    }
    u32 WriteCount = P - Buffer;
    P--;
    while (Buffer <= P)
        *Dest++ = *P-- + '0';
    return WriteCount;
}

internal asset_header *
LoadAssetFile(memory_arena *Arena, char *Filename)
{
    string Content = GlobalPlatform->PushReadFile(Arena, Filename);
    asset_header *Result = (asset_header *)Content.Base;
    
    if (Result)
    {
        glyph *Glyphs = Result->Font.Glyphs;
        for (u32 i = 0; i < ArrayCount(Result->Font.Glyphs); ++i)
        {
            Glyphs[i].Bitmap.Memory = (u8 *)Glyphs[i].Bitmap.Memory + (uintptr_t)Result;
        }
    }
    return Result;
}

internal b32
NewPress(button_state Button)
{
    b32 Result = (Button.IsPressed && !Button.WasPressed);
    return Result;
}

internal b32
NewPressOrRepeat(b32 InputIsPressed, phased_clock *RepState, f32 dt)
{
    b32 Signal = false;
    if (InputIsPressed)
    {
        switch (RepState->Stage)
        {
            case 0:
            {
                Signal = true;
                ++RepState->Stage;
                RepState->t = 0.0f;
            } break;
            case 1:
            {
                RepState->t += dt;
                if (RepState->t >= .24f)
                {
                    Signal = true;
                    RepState->t -= .24f;
                    ++RepState->Stage;
                }
            } break;
            case 2:
            {
                RepState->t += dt;
                if (RepState->t >= .035f)
                {
                    Signal = true;
                    RepState->t -= .035f;
                }
            } break;
            InvalidDefaultCase;
        }
    }
    else
    {
        RepState->t = 0.0f;
        RepState->Stage = 0.0f;
    }
    return Signal;
}

internal v4
GetQuantizationColor(u32 RowCount, u32 RowIndex)
{
    Assert(RowIndex < RowCount);
    Assert(RowCount == 4 || RowCount == 8 || RowCount == 12 || RowCount == 16 || RowCount == 24 ||
           RowCount == 32 || RowCount == 48 || RowCount == 64 || RowCount == 96 || RowCount == 192);
    u32 Div = 192/RowCount;
    u32 NormalizedIndex = RowIndex * Div;
    Assert(NormalizedIndex < 192);
    
    if (NormalizedIndex % (192/4) == 0)  return V4(1,0,0,1);
    if (NormalizedIndex % (192/8) == 0)  return V4(0,0,1,1);
    if (NormalizedIndex % (192/12) == 0) return V4(.5f,.9f,.5f,1);
    if (NormalizedIndex % (192/16) == 0) return V4(0,1,0,1);
    if (NormalizedIndex % (192/24) == 0) return V4(.9f,.3f,.9f,1);
    if (NormalizedIndex % (192/32) == 0) return V4(.9f,.9f,.3f,1);
    if (NormalizedIndex % (192/48) == 0) return V4(.5f,.8f,1,1);
    if (NormalizedIndex % (192/64) == 0) return V4(.9f,.6f,.6f,1);
    return V4(1,1,1,1);
}

struct filenames_node
{
    filenames Filenames;
    u32 CurrentIndex;
};

#define MAX_LOADED_SMFILES_COUNT 20000
internal filenames
GetSMFilenamesInDirTree(string RootPath, memory_arena *Arena, memory_arena *ScratchArena)
{
    
    // TODO(vincent): Make sure this will also work on Windows
    
    Assert(RootPath.Base[RootPath.Size] == 0);
    
    filenames Result = {};
    u32 MaxFilenamesCount = MAX_LOADED_SMFILES_COUNT;
    Result.Strings = PushArray(Arena, MaxFilenamesCount, string);
    
    temporary_memory Scratch = BeginTemporaryMemory(ScratchArena);
    u32 MaxDepth = 100;
    filenames_node *Nodes = PushZeroArray(ScratchArena, MaxDepth, filenames_node);
    u32 DepthIndex = 0;
    
    Nodes[0].Filenames = GlobalPlatform->PushFilenames(ScratchArena, RootPath.Base);
    
    for (;;)
    {
        filenames_node *Node = Nodes + DepthIndex;
        filenames F = Node->Filenames;
        if (F.Count)
        {
            Assert(Node->CurrentIndex < F.Count);
            Assert(RootPath.Base);
            
            string S = F.Strings[Node->CurrentIndex];
            b32 IsDir = F.IsDirectory[Node->CurrentIndex];
            
            if (IsDir)
            {
                if (DepthIndex+1 < MaxDepth)
                {
                    b32 Appended = GlobalPlatform->AppendDirname(&RootPath, S, PATH_BUFFER_SIZE);
                    if (Appended)
                    {
                        filenames_node *NextNode = Node + 1;
                        NextNode->Filenames = GlobalPlatform->PushFilenames(ScratchArena, RootPath.Base);
                        NextNode->CurrentIndex = 0;
                        ++DepthIndex;
                    }
                }
                continue;
            }
            
            // form path, push new filename
            b32 Appended = GlobalPlatform->AppendDirname(&RootPath, S, PATH_BUFFER_SIZE);
            if (Appended)
            {
                Result.Strings[Result.Count].Base = PushArray(Arena, RootPath.Size+1, char);
                Result.Strings[Result.Count].Size = RootPath.Size;
                char *Source = RootPath.Base;
                char *EndSource = RootPath.Base + RootPath.Size;
                char *Dest = Result.Strings[Result.Count].Base;
                while (Source < EndSource)
                    *Dest++ = *Source++;
                *Dest = 0;
                Assert(StringEndsWithCI(Result.Strings[Result.Count].Base, ".sm"));
                ++Result.Count;
                b32 Popped = GlobalPlatform->PopDirname(&RootPath);
                Assert(Popped);
            }
        }
        
        ++Node->CurrentIndex;
        while (Node->CurrentIndex >= Node->Filenames.Count && DepthIndex > 0)
        {
            b32 Popped = GlobalPlatform->PopDirname(&RootPath);
            Assert(Popped);
            --DepthIndex;
            --Node;
            ++Node->CurrentIndex;
        }
        if (DepthIndex == 0 && Node->CurrentIndex >= Node->Filenames.Count)
            break;
    }
    EndTemporaryMemory(Scratch);
    return Result;
}

internal u32
DigitCount(u32 N)
{
    u32 Result = (N == 0 ? 1 : 0);
    while (N > 0)
    {
        N /= 10;
        ++Result;
    }
    return Result;
}


internal b32
StringLessEqual(string A, string B)
{
    b32 Result = true;
    u32 MinSize = Minimum(A.Size, B.Size);
    u32 Offset = 0;
    while (Offset < MinSize)
    {
        char CharA = A.Base[Offset];
        char CharB = B.Base[Offset];
        u32 AValue = ('A' <= CharA && CharA <= 'Z') ? (CharA | 32) : CharA;
        u32 BValue = ('A' <= CharB && CharB <= 'Z') ? (CharB | 32) : CharB;
        if (AValue < BValue)
            break;
        if (AValue > BValue)
        {
            Result = false;
            break;
        }
        ++Offset;
    }
    if (Offset == MinSize && A.Size > B.Size)
        Result = false;
    
    return Result;
}

internal void
TestStringLessEqual(void)
{
    char A[100] = "build";
    char B[100] = "src";
    string SA = {A, 5};
    string SB = {B, 3};
    Assert(StringLessEqual(SA, SB));
}

internal void
SortFilenames(memory_arena *Arena, filenames Filenames)
{
    temporary_memory SortMemory = BeginTemporaryMemory(Arena);
    b32 *IsDirectoryBuffer = PushArray(Arena, Filenames.Count, b32);
    string *StringsBuffer = PushArray(Arena, Filenames.Count, string);
    
#define SwapString(A,B)         string TempString = A; A = B; B = TempString
#define SwapStringPointers(A,B) string *TempSP    = A; A = B; B = TempSP
#define SwapB32(A, B)           b32 TempB32       = A; A = B; B = TempB32
#define SwapB32Pointers(A,B)    b32 *TempB32P     = A; A = B; B = TempB32P
    for (u32 i = 0; i+1 < Filenames.Count; i+=2)
    {
        if (StringLessEqual(Filenames.Strings[i+1], Filenames.Strings[i]))
        {
            SwapString(Filenames.Strings[i], Filenames.Strings[i+1]);
            SwapB32(Filenames.IsDirectory[i], Filenames.IsDirectory[i+1]);
        }
    }
    
    b32 SwappedBuffers = false;
    for (u32 SourceBucketSize = 2; (SourceBucketSize >> 1) < Filenames.Count; SourceBucketSize <<= 1)
    {
        for (u32 BucketOffset = 0; BucketOffset < Filenames.Count; BucketOffset += 2*SourceBucketSize)
        {
            // merge two separately sorted buckets of size <= SourceBucketSize
            string *StringSource1 = Filenames.Strings + BucketOffset;
            b32 *IsDirectorySource1 = Filenames.IsDirectory + BucketOffset;
            s32 Source1Size = Minimum(SourceBucketSize, Filenames.Count - BucketOffset);
            s32 Offset1 = 0;
            
            string *StringSource2 = StringSource1 + SourceBucketSize;
            b32 *IsDirectorySource2 = IsDirectorySource1 + SourceBucketSize;
            s32 Source2Size = Minimum((s32)SourceBucketSize, (s32)Filenames.Count - (s32)BucketOffset - (s32) SourceBucketSize);
            s32 Offset2 = 0;
            
            string *StringDest = StringsBuffer + BucketOffset;
            b32 *IsDirectoryDest = IsDirectoryBuffer + BucketOffset;
            
            while (Offset1 < Source1Size && Offset2 < Source2Size)
            {
                if (StringLessEqual(StringSource1[Offset1], StringSource2[Offset2]))
                {
                    *StringDest++ = StringSource1[Offset1];
                    *IsDirectoryDest++ = IsDirectorySource1[Offset1];
                    ++Offset1;
                }
                else
                {
                    *StringDest++ = StringSource2[Offset2];
                    *IsDirectoryDest++ = IsDirectorySource2[Offset2];
                    ++Offset2;
                }
                
            }
            while (Offset1 < Source1Size)
            {
                *StringDest++ = StringSource1[Offset1];
                *IsDirectoryDest++ = IsDirectorySource1[Offset1];
                ++Offset1;
            }
            while (Offset2 < Source2Size)
            {
                *StringDest++ = StringSource2[Offset2];
                *IsDirectoryDest++ = IsDirectorySource2[Offset2];
                ++Offset2;
            }
            
#if DEBUG
            for (s32 i = 0; i+1 < Offset1+Offset2; ++i)
            {
                Assert(StringLessEqual(StringsBuffer[i], StringsBuffer[i+1]));
            }
#endif
            
        }
        
        SwapB32Pointers(IsDirectoryBuffer, Filenames.IsDirectory);
        SwapStringPointers(StringsBuffer, Filenames.Strings);
        SwappedBuffers = !SwappedBuffers;
    }
    
    if (SwappedBuffers)
    {
        SwapB32Pointers(IsDirectoryBuffer, Filenames.IsDirectory);
        SwapStringPointers(StringsBuffer, Filenames.Strings);
        for (u32 i = 0; i < Filenames.Count; ++i)
        {
            Filenames.IsDirectory[i] = IsDirectoryBuffer[i];
            Filenames.Strings[i] = StringsBuffer[i];
        }
    }
    
#undef SwapString
#undef SwapStringPointers
#undef SwapB32
#undef SwapB32Pointers  
    EndTemporaryMemory(SortMemory);
    
#if DEBUG
    for (u32 i = 0; i+1< Filenames.Count; ++i)
    {
        Assert(StringLessEqual(Filenames.Strings[i], Filenames.Strings[i+1]));
    }
#endif
    
}

internal void
SortDSSongsInfo(memory_arena *Arena, game_state *State)
{
    temporary_memory SortMemory = BeginTemporaryMemory(Arena);
    
    u32 Count = State->AllFiles.Count;
    song_doublestep_info *SDIBuffer = PushArray(Arena, Count, song_doublestep_info);
    song_doublestep_info *SDI = State->SongsDSInfo;
    for (u32 i = 0; i+1 < Count; i+=2)
    {
        if (StringLessEqual(SDI[i+1].Title, SDI[i].Title))
        {
            song_doublestep_info Temp = SDI[i+1];
            SDI[i+1] = SDI[i];
            SDI[i] = Temp;
            //b32 Equal = StringEquals(SDI[i].Title, SDI[i+1].Title);
            //Assert(!Equal);
        }
    }
    
    b32 SwappedBuffers = false;
    for (u32 SourceBucketSize = 2; (SourceBucketSize >> 1) < Count; SourceBucketSize <<= 1)
    {
        for (u32 BucketOffset = 0; BucketOffset < Count; BucketOffset += 2*SourceBucketSize)
        {
            // merge two separately sorted buckets of size <= SourceBucketSize
            song_doublestep_info *Source1 = SDI + BucketOffset;
            s32 Source1Size = Minimum(SourceBucketSize, Count - BucketOffset);
            s32 Offset1 = 0;
            
            song_doublestep_info *Source2 = Source1 + SourceBucketSize;
            s32 Source2Size = Minimum((s32)SourceBucketSize, (s32)Count - (s32)BucketOffset - (s32) SourceBucketSize);
            s32 Offset2 = 0;
            
            song_doublestep_info *Dest = SDIBuffer + BucketOffset;
            
            while (Offset1 < Source1Size && Offset2 < Source2Size)
            {
                if (StringLessEqual(Source1[Offset1].Title, Source2[Offset2].Title))
                    *Dest++ = Source1[Offset1++];
                else
                    *Dest++ = Source2[Offset2++];
            }
            while (Offset1 < Source1Size)
                *Dest++ = Source1[Offset1++];
            while (Offset2 < Source2Size)
                *Dest++ = Source2[Offset2++];
            
#if DEBUG
            for (s32 i = 0; i+1 < Offset1+Offset2; ++i)
            {
                Assert(StringLessEqual(SDIBuffer[i].Title, SDIBuffer[i+1].Title));
                //b32 Equal = StringEquals(SDIBuffer[i].Title, SDIBuffer[i+1].Title);
                //Assert(!Equal);
            }
#endif
        }
        
        song_doublestep_info *Temp = SDIBuffer;
        SDIBuffer = SDI;
        SDI = Temp;
        SwappedBuffers = !SwappedBuffers;
    }
    
    if (SwappedBuffers)
    {
        for (u32 i = 0; i < Count; ++i)
            SDI[i] = SDIBuffer[i];
    }
    
    EndTemporaryMemory(SortMemory);
    
#if DEBUG
    for (u32 i = 0; i+1< State->AllFiles.Count; ++i)
    {
        Assert(StringLessEqual(State->SongsDSInfo[i].Title, State->SongsDSInfo[i+1].Title));
    }
#endif
}


#if 1
internal void
DrawNotes(game_state *State, doublestep *DS)
{
    render_group *Group = &State->RenderGroup;
    font *Font = &State->Assets->Font;
    f32 TextScaling = .0004f;
    
    f32 MeasureYHeight = .2f;
    f32 MeasureWidth = .15f;
    f32 MinX = .5f;
    f32 MaxX = MinX + MeasureWidth;
    f32 MidX = .5f*(MinX+MaxX);
    f32 LaneWidth = .25f*MeasureWidth;
    v4 White = V4(1,1,1,1);
    
    row_text_data *Row = DS->Measure->FirstRow;
    u32 ElapsedRowCount = 0;
    u32 MeasureNoteCount = 0;
    u32 TotalRowCount = GetRowCount(DS->Measure);
    while (Row)
    {
        char C0 = Row->Chars[0];
        char C1 = Row->Chars[1];
        char C2 = Row->Chars[2];
        char C3 = Row->Chars[3];
        if (C0 == '1' || C0 == '2' || C0 == '4' ||
            C1 == '1' || C1 == '2' || C1 == '4' ||
            C2 == '1' || C2 == '2' || C2 == '4' ||
            C3 == '1' || C3 == '2' || C3 == '4')
        {
            ++MeasureNoteCount;
            if (MeasureNoteCount == DS->MeasureNote)
                break;
        }
        Row = Row->Next;
        ++ElapsedRowCount;
    }
    
    v2 NoteDim = V2(.01f, .01f);
    v2 MeasureLineDim = V2(MeasureWidth + .004f, .004f);
    v2 BeatLineDim = V2(MeasureWidth + .002f, .002f);
    v2 HighlightLineDim = V2(MeasureWidth + .006f, .006f);
    
    f32 DoublestepV = SafeRatio0(ElapsedRowCount, TotalRowCount);
    f32 DSMeasureTopY = .5f*Group->ScreenDim.y + State->MeasuresCameraY + DoublestepV * MeasureYHeight;
    f32 FirstMeasureTopY = DSMeasureTopY;
    u32 CurrentMeasureIndex = DS->MeasureIndex;
    measure_text_data *M = DS->Measure;
    
    PushRect(Group, RectCenterDim(V2(MidX, .5f*Group->ScreenDim.y + State->MeasuresCameraY), 
                                  HighlightLineDim), V4(.8f,.7f,.1f, 1));
    
    // NOTE(vincent): This may be expensive if the user moves the camera very far.
    while (FirstMeasureTopY < Group->ScreenDim.y && M->Previous)
    {
        FirstMeasureTopY += MeasureYHeight;
        Assert(CurrentMeasureIndex > 0);
        --CurrentMeasureIndex;
        M = M->Previous;
    }
    while (FirstMeasureTopY - MeasureYHeight > Group->ScreenDim.y && M->Next)
    {
        FirstMeasureTopY -= MeasureYHeight;
        ++CurrentMeasureIndex;
        M = M->Next;
    }
    
    f32 CurrentMeasureTopY = FirstMeasureTopY;
    temporary_memory DrawNotesMemory = BeginTemporaryMemory(&State->MainArena);
    note_draw_info *NoteDrawInfoList = PushFakeStruct(&State->MainArena, note_draw_info);
    
    f32 HoldBeginY[4] = {FirstMeasureTopY, FirstMeasureTopY, FirstMeasureTopY, FirstMeasureTopY};
    
    // NOTE(vincent): Holds and rolls may not be drawn properly when they extend beyond the screen.
    b32 IsHoldNotRoll[4] = {true, true, true, true};
    b32 FinishedHold[4] = {false, false, false, false};
    f32 LaneX[4] = 
    {
        MinX + 0.5f * LaneWidth,
        MinX + 1.5f * LaneWidth,
        MinX + 2.5f * LaneWidth,
        MinX + 3.5f * LaneWidth
    };
    
    while (CurrentMeasureTopY >= -.01f && M)
    {
        f32 LinesY[4];
        LinesY[0] = CurrentMeasureTopY;
        LinesY[1] = CurrentMeasureTopY - .25f*MeasureYHeight;
        LinesY[2] = CurrentMeasureTopY - .50f*MeasureYHeight;
        LinesY[3] = CurrentMeasureTopY - .75f*MeasureYHeight;
        
        PushRect(Group, RectCenterDim(V2(MidX, LinesY[0]), MeasureLineDim), White);
        PushRect(Group, RectCenterDim(V2(MidX, LinesY[1]), BeatLineDim), White);
        PushRect(Group, RectCenterDim(V2(MidX, LinesY[2]), BeatLineDim), White);
        PushRect(Group, RectCenterDim(V2(MidX, LinesY[3]), BeatLineDim), White);
        PushNumber(Group, CurrentMeasureIndex, Font, TextScaling, V2(MinX - .04f, LinesY[0] - .003f), White);
        
        u32 RowCount = GetRowCount(M);
        u32 CurrentRow = 0;
        row_text_data *R = M->FirstRow;
        while (R)
        {
            f32 RowV = SafeRatio0((f32)CurrentRow, (f32)RowCount);
            f32 RowY = CurrentMeasureTopY - RowV*MeasureYHeight;
            v4 RowColor = GetQuantizationColor(RowCount, CurrentRow);
            for (u32 Lane = 0; Lane < 4; ++Lane)
            {
                char C = R->Chars[Lane];
                if (C == '1' || C == '2' || C == '4')
                {
                    note_draw_info *Info = PushStruct(&State->MainArena, note_draw_info);
                    Info->Rect = RectCenterDim(V2(LaneX[Lane], RowY), NoteDim);
                    Info->Color = RowColor;
                }
                
                switch (C)
                {
                    case '2': // begin hold
                    HoldBeginY[Lane] = RowY; 
                    IsHoldNotRoll[Lane] = true; 
                    FinishedHold[Lane] = false; break;
                    case '3': // end hold or roll
                    {
                        v2 MinP = V2(LaneX[Lane], RowY) - .5f*NoteDim;
                        v2 MaxP = V2(LaneX[Lane], HoldBeginY[Lane]) + .5f*NoteDim;
                        PushRect(Group, MinP, MaxP, IsHoldNotRoll[Lane] ? V4(.6f, .6f, .8f, 1) : V4(.4f, .7f, .4f, 1));
                        FinishedHold[Lane] = true;
                    } break;
                    case '4': // begin roll
                    HoldBeginY[Lane] = RowY; 
                    IsHoldNotRoll[Lane] = false; 
                    FinishedHold[Lane] = false; break;
                }
            }
            
            ++CurrentRow;
            R = R->Next;
        }
        
        CurrentMeasureTopY -= MeasureYHeight;
        ++CurrentMeasureIndex;
        M = M->Next;
    }
    
    for (u32 Lane = 0; Lane < 4; ++Lane)
    {
        if (HoldBeginY[Lane] != FirstMeasureTopY && !FinishedHold[Lane])
        {
            v2 MinP = V2(LaneX[Lane], 0) - .5f*NoteDim;
            v2 MaxP = V2(LaneX[Lane], HoldBeginY[Lane]) + .5f*NoteDim;
            PushRect(Group, MinP, MaxP, IsHoldNotRoll[Lane] ? V4(.6f, .6f, .8f, 1) : V4(.4f, .7f, .4f, 1));
        }
    }
    
    note_draw_info *InfoAt = NoteDrawInfoList;
    while ((u8*)InfoAt < State->MainArena.Base + State->MainArena.Used)
    {
        PushRect(Group, InfoAt->Rect, InfoAt->Color);
        ++InfoAt;
    }
    EndTemporaryMemory(DrawNotesMemory);
}
#endif


internal void
GetSongsDSInfo(game_state *State)
{
    State->SongsDSInfo = PushZeroArray(&State->MainArena, State->AllFiles.Count, song_doublestep_info);
    
    for (u32 i = 0; i < State->AllFiles.Count; ++i)
    {
        char *Filename = State->AllFiles.Strings[i].Base;
        string SMText = GlobalPlatform->PushReadFile(&State->MainArena, Filename);
        Assert(SMText.Base);
        smfile_text_data ParsedText = ParseSMFile(SMText, &State->MainArena);
        smfile_header *Header = ParsedText.FirstHeader;
        while (Header)
        {
            if (StringEquals(Header->Name, "TITLE"))
            {
                State->SongsDSInfo[i].Title = Header->Value;
                break;
            }
            Header = Header->Next;
        }
        chart_text_data *Chart = ParsedText.FirstChart;
        chart_doublestep_info *CDI = 0;
        while (Chart)
        {
            if (CDI)
            {
                CDI->Next = PushZeroStruct(&State->MainArena, chart_doublestep_info);
                CDI = CDI->Next;
            }
            else
            {
                CDI = PushZeroStruct(&State->MainArena, chart_doublestep_info);
                State->SongsDSInfo[i].FirstChartInfo = CDI;
            }
            
            CDI->ChartInfo.Size = Chart->Headers[0].Size + 1 + Chart->Headers[1].Size + 1
                + Chart->Headers[2].Size + 1 + Chart->Headers[3].Size;
            
            char *Dest = PushArray(&State->MainArena, CDI->ChartInfo.Size+1, char);
            CDI->ChartInfo.Base = Dest;
            
            char *Source = Chart->Headers[0].Base;
            for (u32 i = 0; i < Chart->Headers[0].Size; ++i)
                *Dest++ = *Source++;
            *Dest++ = ' ';
            Source = Chart->Headers[2].Base;
            for (u32 i = 0; i < Chart->Headers[2].Size; ++i)
                *Dest++ = *Source++;
            *Dest++ = ' ';
            Source = Chart->Headers[3].Base;
            for (u32 i = 0; i < Chart->Headers[3].Size; ++i)
                *Dest++ = *Source++;
            *Dest++ = ' ';
            Source = Chart->Headers[1].Base;
            for (u32 i = 0; i < Chart->Headers[1].Size; ++i)
                *Dest++ = *Source++;
            *Dest = 0;
            Assert(CDI->ChartInfo.Base + CDI->ChartInfo.Size == Dest);
            
            measure_text_data *Measure = Chart->FirstMeasure;
            u32 MeasureCount = 0;
            feet_state FeetState = {};
            FeetState.LocationRight = 3;
            u32 LaneState[4] = {}; // 0: init, 1: holding, 2: rolling
            doublestep *Doublestep = 0;
            while (Measure)
            {
                ++MeasureCount;
                row_text_data *Row = Measure->FirstRow;
                u32 MeasureNoteCount = 0;
                while (Row)
                {
                    char C0 = Row->Chars[0];
                    char C1 = Row->Chars[1];
                    char C2 = Row->Chars[2];
                    char C3 = Row->Chars[3];
                    
                    u32 HitPattern = 
                    ((C0 == '1' || C0 == '2' || C0 == '4') << 0) |
                    ((C1 == '1' || C1 == '2' || C1 == '4') << 1) |
                    ((C2 == '1' || C2 == '2' || C2 == '4') << 2) |
                    ((C3 == '1' || C3 == '2' || C3 == '4') << 3);
                    
                    if (HitPattern)
                    {
                        foot_type MovingFoot;
                        b32 FoundDoublestep = false;
                        ++MeasureNoteCount;
                        
                        switch (HitPattern)
                        {
                            case 1: // left arrow
                            {
                                if (FeetState.LastMoved == FootType_Left)
                                {
                                    if (!FeetState.RightIsGlued)
                                    {
                                        FoundDoublestep = true;
                                        MovingFoot = FootType_Left;
                                    }
                                }
                                else if (FeetState.LastMoved == FootType_PossiblyLeft)
                                {
                                    Assert(FeetState.LocationLeft != 0);
                                    FeetState.LocationRight = FeetState.LocationLeft;
                                }
                                FeetState.LocationLeft = 0;
                                FeetState.LastMoved = FootType_Left;
                            } break;
                            
                            case 8: // right arrow
                            {
                                if (FeetState.LastMoved == FootType_Right)
                                {
                                    if (!FeetState.LeftIsGlued)
                                    {
                                        FoundDoublestep = true;
                                        MovingFoot = FootType_Right;
                                    }
                                }
                                else if (FeetState.LastMoved == FootType_PossiblyRight)
                                {
                                    Assert(FeetState.LocationRight != 3);
                                    FeetState.LocationLeft = FeetState.LocationRight;
                                }
                                FeetState.LocationRight = 3;
                                FeetState.LastMoved = FootType_Right;
                            } break;
                            
                            case 2: // down arrow
                            case 4: // up arrow
                            {
                                u32 Dest = HitPattern >> 1;
                                if (FeetState.LastMoved == FootType_Jump)
                                {
                                    if (FeetState.LocationLeft == Dest)
                                        FeetState.LastMoved = FootType_Left;
                                    else if (FeetState.LocationRight == Dest)
                                        FeetState.LastMoved = FootType_Right;
                                    else if (FeetState.LeftIsGlued && FeetState.RightIsGlued) {}
                                    else if (FeetState.LeftIsGlued)
                                    {
                                        FeetState.LastMoved = FootType_Right;
                                        FeetState.LocationRight = Dest;
                                    }
                                    else if (FeetState.RightIsGlued)
                                    {
                                        FeetState.LastMoved = FootType_Left;
                                        FeetState.LocationLeft = Dest;
                                    }
                                    else
                                    {
                                        FeetState.LocationLeft = Dest;
                                        FeetState.LastMoved = FootType_PossiblyLeft;
                                    }
                                }
                                
                                else if (State->TreatUUDDAsFootswitches)
                                {
                                    switch (FeetState.LastMoved)
                                    {
                                        case FootType_Left:
                                        if (FeetState.RightIsGlued && !FeetState.LeftIsGlued)
                                            FeetState.LocationLeft = Dest;
                                        else
                                        {
                                            FeetState.LocationRight = Dest;
                                            FeetState.LastMoved = FootType_Right; 
                                        } break;
                                        case FootType_Right:
                                        if (FeetState.LeftIsGlued && !FeetState.RightIsGlued)
                                            FeetState.LocationRight = Dest;
                                        else
                                        {
                                            FeetState.LocationLeft = Dest;
                                            FeetState.LastMoved = FootType_Left; 
                                        } break;
                                        case FootType_PossiblyLeft:
                                        if (FeetState.RightIsGlued && !FeetState.LeftIsGlued)
                                        {
                                            FeetState.LocationLeft = Dest;
                                            FeetState.LastMoved = FootType_Left;
                                        }
                                        else
                                        {
                                            FeetState.LocationRight = Dest;
                                            FeetState.LastMoved = FootType_PossiblyRight; 
                                        } break;
                                        case FootType_PossiblyRight:
                                        if (FeetState.LeftIsGlued && !FeetState.RightIsGlued)
                                        {
                                            FeetState.LocationRight = Dest;
                                            FeetState.LastMoved = FootType_Right;
                                        }
                                        else
                                        {
                                            FeetState.LocationLeft = Dest;
                                            FeetState.LastMoved = FootType_PossiblyLeft; 
                                        } break;
                                        InvalidDefaultCase;
                                    }
                                }
                                
                                else
                                {
                                    switch (FeetState.LastMoved)
                                    {
                                        case FootType_Left:
                                        if (FeetState.LocationLeft == Dest)
                                        {
                                            if (!FeetState.RightIsGlued)
                                            {
                                                FoundDoublestep = true;
                                                MovingFoot = FootType_Left;
                                            }
                                        } 
                                        else
                                        {
                                            if (FeetState.RightIsGlued)
                                                FeetState.LocationLeft = Dest;
                                            else
                                            {
                                                FeetState.LocationRight = Dest;
                                                FeetState.LastMoved = FootType_Right;
                                            }
                                        } break;
                                        
                                        case FootType_Right:
                                        if (FeetState.LocationRight == Dest)
                                        {
                                            if (!FeetState.LeftIsGlued)
                                            {
                                                FoundDoublestep = true;
                                                MovingFoot = FootType_Right;
                                            }
                                        }
                                        else
                                        {
                                            if (FeetState.LeftIsGlued)
                                                FeetState.LocationRight = Dest;
                                            else
                                            {
                                                FeetState.LocationLeft = Dest;
                                                FeetState.LastMoved = FootType_Left;
                                            }
                                        } break;
                                        
                                        case FootType_PossiblyLeft:
                                        if (FeetState.LocationLeft == Dest)
                                        {
                                            if (!FeetState.RightIsGlued)
                                            {
                                                FoundDoublestep = true;
                                                MovingFoot = FootType_Unknown;
                                            }
                                        }
                                        else
                                        {
                                            if (FeetState.RightIsGlued)
                                                FeetState.LocationLeft = Dest;
                                            else
                                            {
                                                FeetState.LocationRight = Dest;
                                                FeetState.LastMoved = FootType_PossiblyRight;
                                            }
                                        } break;
                                        case FootType_PossiblyRight:
                                        if (FeetState.LocationRight == Dest)
                                        {
                                            if (!FeetState.LeftIsGlued)
                                            {
                                                FoundDoublestep = true;
                                                MovingFoot = FootType_Unknown;
                                            }
                                        } 
                                        else
                                        {
                                            if (FeetState.LeftIsGlued)
                                                FeetState.LocationRight = Dest;
                                            else
                                            {
                                                FeetState.LocationLeft = Dest;
                                                FeetState.LastMoved = FootType_PossiblyLeft;
                                            }
                                        } break;
                                        InvalidDefaultCase;
                                    }
                                }
                            } break;
                            
                            case 3: // LD
                            FeetState.LastMoved = FootType_Jump;
                            FeetState.LocationLeft = 0;
                            FeetState.LocationRight = 1; break;
                            case 5: // LU
                            FeetState.LastMoved = FootType_Jump;
                            FeetState.LocationLeft = 0;
                            FeetState.LocationRight = 2; break;
                            case 9: // LR
                            FeetState.LastMoved = FootType_Jump;
                            FeetState.LocationLeft = 0;
                            FeetState.LocationRight = 3; break;
                            case 10: // DR
                            FeetState.LastMoved = FootType_Jump;
                            FeetState.LocationLeft = 1;
                            FeetState.LocationRight = 3; break;
                            case 12: // UR
                            FeetState.LastMoved = FootType_Jump;
                            FeetState.LocationLeft = 0;
                            FeetState.LocationRight = 1; break;
                            
                            case 6: // DU
                            FeetState.LastMoved = FootType_Jump;
                            if (FeetState.LocationLeft == 1)
                                FeetState.LocationRight = 2;
                            else if (FeetState.LocationLeft == 2)
                                FeetState.LocationRight = 1;
                            else if (FeetState.LocationRight == 1)
                                FeetState.LocationLeft = 2;
                            else if (FeetState.LocationRight == 2)
                                FeetState.LocationLeft = 1;
                            else
                            {
                                // treat this like it's the beginning of the chart
                                FeetState.LocationLeft = 0;
                                FeetState.LocationRight = 3;
                            } break;
                            
                            case 7:  // LDU
                            case 11: // LDR
                            case 13: // LUR
                            case 14: // DUR
                            case 15: // LDUR
                            FeetState.LastMoved = FootType_Jump;
                            FeetState.LocationLeft = 0;
                            FeetState.LocationRight = 3; break;
                            InvalidDefaultCase;
                        }
                        
                        if (FoundDoublestep)
                        {
                            if (Doublestep)
                            {
                                Doublestep->Next = PushZeroStruct(&State->MainArena, doublestep);
                                Doublestep = Doublestep->Next;
                            }
                            else
                            {
                                Doublestep = PushZeroStruct(&State->MainArena, doublestep);
                                CDI->FirstDS = Doublestep;
                            }
                            Doublestep->MeasureIndex = MeasureCount-1;
                            Doublestep->MeasureNote = MeasureNoteCount;
                            Doublestep->Measure = Measure;
                            Doublestep->MovingFoot = MovingFoot;
                            
                            // mMeasureIndex nMeasureNote MovingFoot
                            u32 DigitCountMeasureIndex = DigitCount(Doublestep->MeasureIndex);
                            u32 DigitCountMeasureNote = DigitCount(Doublestep->MeasureNote);
                            u32 CharCountMovingFoot;
                            switch (MovingFoot)
                            {
                                case FootType_Left:    CharCountMovingFoot = 4; break;
                                case FootType_Right:   CharCountMovingFoot = 5; break;
                                case FootType_Unknown: CharCountMovingFoot = 7; break;
                                InvalidDefaultCase;
                            }
                            u32 DSStringSize = 1+DigitCountMeasureIndex+2+DigitCountMeasureNote+1+CharCountMovingFoot;
                            Doublestep->String.Base = PushArray(&State->MainArena, DSStringSize, char);
                            Doublestep->String.Size = DSStringSize;
                            char *Dest = Doublestep->String.Base;
                            *Dest++ = 'm';
                            Dest += WriteNumber(Dest, Doublestep->MeasureIndex);
                            *Dest++ = ' ';
                            *Dest++ = 'n';
                            Dest += WriteNumber(Dest, Doublestep->MeasureNote);
                            *Dest++ = ' ';
                            switch (MovingFoot)
                            {
                                case FootType_Left:    Dest += WriteString(Dest, "left");  break;
                                case FootType_Right:   Dest += WriteString(Dest, "right"); break;
                                case FootType_Unknown: Dest += WriteString(Dest, "unknown"); break;
                                InvalidDefaultCase;
                            }
                            Assert(Doublestep->String.Base + DSStringSize == Dest);
                        }
                        
                        
                    } // END if (HitPattern)
                    
                    
                    u32 RollingCount = 0;
                    u32 HoldingCount = 0;
                    for (u32 Lane = 0; Lane < 4; ++Lane)
                    {
                        char C = Row->Chars[Lane];
                        switch (C)
                        {
                            case '2':
                            if (State->GlueFeetOnHolds)
                            {
                                if (FeetState.LocationLeft == Lane)
                                    FeetState.LeftIsGlued = true;
                                if (FeetState.LocationRight == Lane)
                                    FeetState.RightIsGlued = true; 
                            } 
                            LaneState[Lane] = 1; break;
                            
                            case '3':
                            if (FeetState.LocationLeft == Lane)
                                FeetState.LeftIsGlued = false;
                            if (FeetState.LocationRight == Lane)
                                FeetState.RightIsGlued = false;
                            LaneState[Lane] = 0; break;
                            
                            case '4':
                            if (State->GlueFeetOnRolls)
                            {
                                if (FeetState.LocationLeft == Lane)
                                    FeetState.LeftIsGlued = false;
                                if (FeetState.LocationRight == Lane)
                                    FeetState.RightIsGlued = false;
                            } 
                            LaneState[Lane] = 2; break;
                        }
                        
                        if (LaneState[Lane] == 1)
                            ++HoldingCount;
                        if (LaneState[Lane] == 2)
                            ++RollingCount;
                    }
                    
                    b32 ShouldReleaseFeet = false;
                    if (State->GlueFeetOnRolls || State->GlueFeetOnHolds)
                    {
                        if (!State->GlueFeetOnRolls)
                        {
                            if (HoldingCount == 0)
                                ShouldReleaseFeet = true;
                        }
                        if (!State->GlueFeetOnHolds)
                        {
                            if (RollingCount == 0)
                                ShouldReleaseFeet = true;
                        }
                        if (RollingCount == 0 && HoldingCount == 0)
                            ShouldReleaseFeet = true;
                    }
                    if (ShouldReleaseFeet)
                    {
                        FeetState.LeftIsGlued = false;
                        FeetState.RightIsGlued = false;
                    }
                    
                    
                    Row = Row->Next;
                }
                Measure = Measure->Next;
            }
            Chart = Chart->Next;
        }
        
        if (State->MainArena.Used + Megabytes(50) > State->MainArena.Size)
        {
            State->AllFiles.Count = i+1;
            State->ExitedEarly = true;
            break;
        }
        
    }
    SortDSSongsInfo(&State->MainArena, State);
}

internal b32
DrawOptionsUI(game_state *State, game_input *Input)
{
    b32 ChangedOptions = false;
    font *Font = &State->Assets->Font;
    render_group *Group = &State->RenderGroup;
    v2 MouseVector = Hadamard(V2(Input->MouseX, Input->MouseY), Group->ScreenDim);
    f32 TextScaling = .00045f;
    
    char *Strings[] =
    {
        "glue feet on holds",
        "glue feet on rolls",
        "treat UU/DD as footswitches",
    };
    v4 White = V4(1,1,1,1);
    v4 Black = V4(0,0,0,1);
    f32 BoxX = .75f;
    v2 BoxDim = V2(.008f, .008f);
    v2 InnerBoxDim = V2(.006f, .006f);
    f32 LineDeltaY = .014f;
    f32 LineTopY = .4f;
    f32 LineAt = LineTopY;
    
    for (u32 i = 0; i < ArrayCount(Strings); ++i)
    {
        b32 *OptionPointer = 0;
        switch (i)
        {
            case 0: OptionPointer = &State->GlueFeetOnHolds; break;
            case 1: OptionPointer = &State->GlueFeetOnRolls; break;
            case 2: OptionPointer = &State->TreatUUDDAsFootswitches; break;
            InvalidDefaultCase;
        }
        v2 BoxCenter = V2(BoxX, LineAt + .5f*BoxDim.y);
        rectangle2 InnerBox = RectCenterDim(BoxCenter, InnerBoxDim);
        rectangle2 Box = RectCenterDim(BoxCenter, BoxDim);
        v2 TextP = V2(BoxX + .01f, LineAt);
        rectangle2 TextRect = GetTextRectangle(Strings[i], Font, TextScaling, TextP);
        rectangle2 ClickBox = RectMinMax(Box.Min, V2(TextRect.Max.x, Box.Max.y));
        ClickBox.Min -= V2(.003f, .003f);
        ClickBox.Max += V2(.003f, .003f);
        if ((NewPress(Input->MouseLeft) && IsInRectangle(ClickBox, MouseVector)) ||
            (i == 0 && NewPress(Input->X)) || 
            (i == 1 && NewPress(Input->Y)) || 
            (i == 2 && NewPress(Input->B)))
        {
            *OptionPointer ^= 1;
            ChangedOptions = true;
        }
        PushRect(Group, Box, White);
        PushRect(Group, InnerBox, *OptionPointer ? White : Black);
        PushText(Group, Strings[i], Font, TextScaling, V2(BoxX+.01f, LineAt), White);
        LineAt -= LineDeltaY;
    }
    
    return ChangedOptions;
}

extern "C"
GAME_UPDATE(GameUpdate)
{
    game_state *State = (game_state *)Memory->Storage;
    GlobalPlatform = &Memory->Platform;
    
    TestStringLessEqual();
    
    if (!State->IsInitialized)
    {
        Assert(Memory->StorageSize >= 2*sizeof(game_state));
        InitializeArena(&State->MainArena, Memory->StorageSize - sizeof(game_state),
                        (u8 *)Memory->Storage + sizeof(game_state));
        State->RenderGroup = SetRenderGroup(RenderCommands);
        State->Assets = LoadAssetFile(&State->MainArena, "dsfinder_asset_file");
        State->IsInitialized = true;
        
        State->Path.Base = PushArray(&State->MainArena, PATH_BUFFER_SIZE, char);
        GlobalPlatform->GetInitialPath(&State->Path, PATH_BUFFER_SIZE);
        Assert(State->Path.Size > 0);
        
        State->FilenamesMemory = BeginTemporaryMemory(&State->MainArena);
        State->Filenames = GlobalPlatform->PushFilenames(&State->MainArena, State->Path.Base);
        
        // NOTE(vincent): .sm file size reference:
        // - PEMC: 513kB
        // - Stratospheric Intricacy: 422kB
        // - Crapyard Scent 24h of 100bpm: 6681kB
        // - 200,000 steps challenge: 2322kB
        
        State->GameMode = GameMode_FileExplorer;
    }
    
    render_group *Group = &State->RenderGroup;
    font *Font = &State->Assets->Font;
    f32 TextScaling = 0.0004f;
    f32 LineHeight = .01f;
    
    v4 White = V4(1,1,1,1);
    v4 Grey = V4(.4f,.4f,.4f,1);
    v2 MouseVector = Hadamard(V2(Input->MouseX, Input->MouseY), Group->ScreenDim);
    
    PushClear(Group, V4(0.1f,0.1f,0.1f,1));
    
    b32 PressingToMoveDown = Input->Down.IsPressed || Input->ThumbLY <= -.5f;
    b32 PressingToMoveUp = Input->Up.IsPressed || Input->ThumbLY >= .5f;
    
    switch (State->GameMode)
    {
        case GameMode_FileExplorer:
        {
            
            if (NewPress(Input->A))
            {
                State->GameMode = GameMode_DSFinder;
            }
            
            if (NewPressOrRepeat(PressingToMoveDown, &State->DownRepState, Input->dtForFrame))
            {
                if (State->FilenameCursorIndex + 1 < State->Filenames.Count)
                    ++State->FilenameCursorIndex;
            }
            
            if (NewPressOrRepeat(PressingToMoveUp, &State->UpRepState, Input->dtForFrame))
            {
                if (0 < State->FilenameCursorIndex)
                    --State->FilenameCursorIndex;
            }
            
            if (NewPress(Input->Right))
            {
                if (State->Filenames.IsDirectory[State->FilenameCursorIndex])
                {
                    string S = State->Filenames.Strings[State->FilenameCursorIndex];
                    b32 Appended = GlobalPlatform->AppendDirname(&State->Path, S, PATH_BUFFER_SIZE);
                    if (Appended)
                    {
                        EndTemporaryMemory(State->FilenamesMemory);
                        State->FilenamesMemory = BeginTemporaryMemory(&State->MainArena);
                        State->Filenames = GlobalPlatform->PushFilenames(&State->MainArena, State->Path.Base);
                        if (State->Filenames.Count == 0)
                        {
                            State->ShowMessageEmptyDirClock = 1.5f;
                            b32 Popped = GlobalPlatform->PopDirname(&State->Path);
                            Assert(Popped);
                            EndTemporaryMemory(State->FilenamesMemory);
                            State->FilenamesMemory = BeginTemporaryMemory(&State->MainArena);
                            State->Filenames = GlobalPlatform->PushFilenames(&State->MainArena, State->Path.Base);
                        }
                        else
                        {
                            State->FilenameCursorIndex = 0;
                            SortFilenames(&State->MainArena, State->Filenames);
                        }
                    }
                }
            }
            
            if (NewPress(Input->Left))
            {
                b32 Popped = GlobalPlatform->PopDirname(&State->Path);
                if (Popped)
                {
                    EndTemporaryMemory(State->FilenamesMemory);
                    State->FilenamesMemory = BeginTemporaryMemory(&State->MainArena);
                    State->Filenames = GlobalPlatform->PushFilenames(&State->MainArena, State->Path.Base);
                    State->FilenameCursorIndex = 0;
                    SortFilenames(&State->MainArena, State->Filenames);
                }
            }
            
            f32 CWDRectHeight = .013f;
            PushRect(Group, V2(0, Group->ScreenDim.y - CWDRectHeight), Group->ScreenDim, V4(.8f, .8f, .3f, 1.0f));
            PushText(Group, State->Path, Font, TextScaling, V2(0, Group->ScreenDim.y - LineHeight), V4(.1f,.1f,.1f,1));
            
            f32 FilenamesTopY = Group->ScreenDim.y - CWDRectHeight - LineHeight - .003f;
            u32 MaxFilenameDrawCount = (u32)(FilenamesTopY / LineHeight);
            
            State->FilenameDrawTopIndex = Minimum(State->FilenameDrawTopIndex, State->FilenameCursorIndex);
            if (State->FilenameCursorIndex >= State->FilenameDrawTopIndex + MaxFilenameDrawCount)
                ++State->FilenameDrawTopIndex;
            
            f32 FilenamesDrawY = FilenamesTopY;
            for (u32 i = State->FilenameDrawTopIndex; 
                 i < State->Filenames.Count && 0.0f <= FilenamesDrawY - LineHeight; 
                 ++i)
            {
                string S = State->Filenames.Strings[i];
                if (i == State->FilenameCursorIndex)
                {
                    rectangle2 TextRect = GetTextRectangle(S, Font, TextScaling, V2(0, FilenamesDrawY));
                    f32 GrowRadius = .1f*LineHeight;
                    TextRect.Min -= V2(GrowRadius, GrowRadius);
                    TextRect.Max += V2(GrowRadius, GrowRadius);
                    PushRect(Group, TextRect, Grey);
                    break;
                }
                FilenamesDrawY -= LineHeight;
            }
            FilenamesDrawY = FilenamesTopY;
            for (u32 i = State->FilenameDrawTopIndex; 
                 i < State->Filenames.Count && 0.0f <= FilenamesDrawY - LineHeight; 
                 ++i)
            {
                string S = State->Filenames.Strings[i];
                PushText(Group, S, Font, TextScaling, V2(0, FilenamesDrawY) , White);
                FilenamesDrawY -= LineHeight;
            }
            
            if (State->ShowMessageEmptyDirClock > 0.0f)
            {
                State->ShowMessageEmptyDirClock -= Input->dtForFrame;
                PushText(Group, "Directory has no subdirectories or .sm files", Font, TextScaling, 
                         V2(.7f*Group->ScreenDim.x, .93f*Group->ScreenDim.y), V4(1,0,0,0));
            }
            
            
        } break;
        
        case GameMode_DSFinder:
        {
#if 1
            if (!State->DSFinderIsInitialized)
            {
                State->DSFinderIsInitialized = true;
                State->DSFinderMemory = BeginTemporaryMemory(&State->MainArena);
                SubArena(&State->SearchArena, &State->MainArena, Megabytes(20));
                State->AllFiles = GetSMFilenamesInDirTree(State->Path, &State->MainArena,
                                                          &State->SearchArena);
                //Assert(State->AllFiles.Count > 0);
#if DEBUG
                for (u32 i = 0; i < State->AllFiles.Count; ++i)
                    Assert(StringEndsWithCI(State->AllFiles.Strings[i].Base, ".sm"));
#endif
                GetSongsDSInfo(State);
                
                // NOTE(vincent): Form an array of dsfinder_line
                State->LinesCount = 0;
                State->DSCount = 0;
                State->Lines = PushFakeStruct(&State->MainArena, dsfinder_line);
                for (u32 i = 0; i < State->AllFiles.Count; ++i)
                {
                    song_doublestep_info *SDI = State->SongsDSInfo + i;
                    dsfinder_line *TitleLine = PushStruct(&State->MainArena, dsfinder_line);
                    TitleLine->String = SDI->Title;
                    TitleLine->Type = DSBusterLine_Title;
                    for (chart_doublestep_info *CDI = SDI->FirstChartInfo; CDI; CDI = CDI->Next)
                    {
                        dsfinder_line *ChartInfoLine = PushStruct(&State->MainArena, dsfinder_line);
                        ChartInfoLine->String = CDI->ChartInfo;
                        ChartInfoLine->Type = DSBusterLine_ChartInfo;
                        for (doublestep *DS = CDI->FirstDS; DS; DS = DS->Next)
                        {
                            ++State->DSCount;
                            dsfinder_line *DSLine = PushStruct(&State->MainArena, dsfinder_line);
                            DSLine->String = DS->String;
                            DSLine->Type = DSBusterLine_DS;
                            DSLine->DS = DS;
                        }
                    }
                }
                State->LinesCount = PushFakeStruct(&State->MainArena, dsfinder_line) - State->Lines;
                //Assert(State->LinesCount > 0);
                State->CurrentLine = 0;
                State->TopLine = 0;
            }
            
            b32 ChangedOptions = DrawOptionsUI(State, Input);
            
            if (ChangedOptions)
            {
                State->DSFinderIsInitialized = false;
                State->ExitedEarly = false;
                EndTemporaryMemory(State->DSFinderMemory);
                Assert(State->MainArena.Used < Kilobytes(200));
            }
            else if (NewPress(Input->A))
            {
                State->GameMode = GameMode_FileExplorer;
                State->DSFinderIsInitialized = false;
                State->ExitedEarly = false;
                EndTemporaryMemory(State->DSFinderMemory);
                Assert(State->MainArena.Used < Kilobytes(200));
            }
            else
            {
                f32 TopY = Group->ScreenDim.y - LineHeight;
                f32 CurrentY = TopY;
                //Assert(State->AllFiles.Count > 0);
                
                if (NewPressOrRepeat(PressingToMoveUp, &State->UpRepState, Input->dtForFrame))
                {
                    if (State->CurrentLine > 0)
                    {
                        --State->CurrentLine;
                        if (State->TopLine > State->CurrentLine)
                            State->TopLine = State->CurrentLine;
                    }
                    State->MeasuresCameraY = 0.0f;
                }
                if (NewPressOrRepeat(PressingToMoveDown, &State->DownRepState, Input->dtForFrame))
                {
                    if (State->CurrentLine + 1 < State->LinesCount)
                    {
                        ++State->CurrentLine;
                        if (State->TopLine + 45 < State->CurrentLine)
                            ++State->TopLine;
                    }
                    State->MeasuresCameraY = 0.0f;
                }
                
                if (NewPress(Input->MouseLeft))
                {
                    State->LastClickY = MouseVector.y;
                    State->MeasuresCameraYLastClick = State->MeasuresCameraY;
                }
                if (Input->MouseLeft.IsPressed)
                    State->MeasuresCameraY = State->MeasuresCameraYLastClick + (MouseVector.y - State->LastClickY);
                
                State->MeasuresCameraY -= .012f*Input->ThumbRY;
                
                // NOTE(vincent): Draw selection rectangle in text menu
                for (u32 i = State->TopLine; i < State->LinesCount && 0.0f <= CurrentY; ++i)
                {
                    dsfinder_line *Line = State->Lines + i;
                    if (i > 0 && Line->Type == DSBusterLine_Title)
                        CurrentY -= .5f * LineHeight;
                    
                    if (i == State->CurrentLine)
                    {
                        rectangle2 TextRect = GetTextRectangle(State->Lines[State->CurrentLine].String, Font, TextScaling, 
                                                               V2(0, CurrentY));
                        f32 GrowRadius = 0.1 * LineHeight;
                        TextRect.Min.x -= GrowRadius;
                        TextRect.Min.y -= GrowRadius;
                        TextRect.Max.x += GrowRadius;
                        TextRect.Max.y += GrowRadius;
                        PushRect(Group, TextRect, V4(.4f, .4f, .4f, 1.0f));
                        break;
                    }
                    CurrentY -= LineHeight;
                }
                
                // NOTE(vincent): Draw text menu
                CurrentY = TopY;
                for (u32 i = State->TopLine; i < State->LinesCount && 0.0f <= CurrentY; ++i)
                {
                    dsfinder_line *Line = State->Lines + i;
                    if (i > 0 && Line->Type == DSBusterLine_Title)
                        CurrentY -= .5f * LineHeight;
                    v4 Color = V4(1,1,1,1);
                    switch (Line->Type)
                    {
                        case DSBusterLine_Title: Color = V4(.6f, 1, .4f, 1); break;
                        case DSBusterLine_ChartInfo: Color = V4(.5f, .7f, 1, 1); break;
                        case DSBusterLine_DS: break;
                        InvalidDefaultCase;
                    }
                    
                    PushText(Group, Line->String, Font, TextScaling, V2(0, CurrentY), Color);
                    CurrentY -= LineHeight;
                }
                
                // NOTE(vincent): Draw measures and notes
                dsfinder_line *SelectedLine = State->Lines + State->CurrentLine;
                if (SelectedLine->Type == DSBusterLine_DS)
                    DrawNotes(State, SelectedLine->DS);
                
                // NOTE(vincent): Messages
                char LoadedFilesCountMessage[50];
                char *Dest = LoadedFilesCountMessage;
                Dest += WriteNumber(Dest, State->AllFiles.Count);
                Dest += WriteString(Dest, " loaded files, ");
                Dest += WriteNumber(Dest, State->DSCount);
                Dest += WriteString(Dest, " doublesteps");
                *Dest = 0;
                PushText(Group, LoadedFilesCountMessage, Font, .8f*TextScaling, V2(.8f, .11f), White);
                
                if (State->AllFiles.Count >= MAX_LOADED_SMFILES_COUNT)
                {
                    char MaxFilesLoadedMessage1[50] = "Reached max sm files count,";
                    char MaxFilesLoadedMessage2[50] = "may have ignored some files";
                    PushText(Group, MaxFilesLoadedMessage1, Font, .8f*TextScaling, V2(.8f, .14f), V4(1,0,0,1));
                    PushText(Group, MaxFilesLoadedMessage2, Font, .8f*TextScaling, V2(.8f, .13f), V4(1,0,0,1));
                    
                }
                if (State->ExitedEarly)
                {
                    char EarlyExitMessage1[50] = "Parser interrupted (out of memory),";
                    char EarlyExitMessage2[50] = "may have ignored some files";
                    PushText(Group, EarlyExitMessage1, Font, .8f*TextScaling, V2(.8f, .1f), V4(1,0,0,1));
                    PushText(Group, EarlyExitMessage2, Font, .8f*TextScaling, V2(.8f, .1f -.8f*LineHeight), V4(1,0,0,1));
                }
#endif
            }
            
        } break;
        
        InvalidDefaultCase;
    }
    
    char MemoryUsedMessage[30] = {0};
    char *Dest = MemoryUsedMessage;
    Dest += WriteString(Dest, "Memory used: ");
    Dest += WriteNumber(Dest, State->MainArena.Used / 1000);
    Dest += WriteString(Dest, "kB");
    PushText(Group, MemoryUsedMessage, Font, .8f*TextScaling, V2(.8f, .08f), White);
    v2 GrowRadiusBG = V2(.0015f, .0015f);
    v2 RectMin = V2(.8015f, .065f);
    v2 RectMax = V2(.9f, .074f);
    f32 FillX = Lerp(RectMin.x, (f32)State->MainArena.Used / (f32)State->MainArena.Size, RectMax.x);
    PushRect(Group, RectMin - GrowRadiusBG, RectMax + GrowRadiusBG, V4(.3f, .3f, .3f, 1));
    PushRect(Group, RectMin, RectMax, V4(.1f, .1f, .1f, 1));
    PushRect(Group, RectMin, V2(FillX, RectMax.y), V4(.4f, .8f, .1f, 1));
}



