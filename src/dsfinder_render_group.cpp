struct render_group
{
    v2 ScreenDim;
    f32 PixelsPerMeter;
    game_render_commands *Commands;
};

inline render_group
SetRenderGroup(game_render_commands *Commands)
{
    render_group Result = {};
    f32 MonitorWidth = 1.0f;
    Result.PixelsPerMeter = Commands->Width / MonitorWidth;
    f32 MonitorHeight = Commands->Height / Result.PixelsPerMeter;
    Result.ScreenDim = V2(MonitorWidth, MonitorHeight);
    Result.Commands = Commands;
    
    return Result;
}

#define PushRenderElement(Group, type) (type *)PushRenderElement_(Group, sizeof(type), RenderEntryType_##type)

inline void *
PushRenderElement_(render_group *Group, u32 Size, render_entry_type Type)
{
    game_render_commands *Commands = Group->Commands;
    void *Result = 0;
    Size += sizeof(render_entry_header);
    Assert(Commands->PushBufferSize + Size <= Commands->MaxPushBufferSize);
    render_entry_header *Header = (render_entry_header *)(Commands->PushBufferBase + Commands->PushBufferSize);
    Header->Type = Type;
    Result = (u8 *)Header + sizeof(*Header);
    Commands->PushBufferSize += Size;
    return Result;
}

inline render_entry_bitmap *
PushBitmap(render_group *Group, bitmap *Bitmap, v2 Origin, v2 XAxis, v2 YAxis, v4 Color)
{
    render_entry_bitmap *Entry = PushRenderElement(Group, render_entry_bitmap);
    if (Entry)
    {
        Entry->Origin = Origin * Group->PixelsPerMeter;
        Entry->XAxis = XAxis * Group->PixelsPerMeter;
        Entry->YAxis = YAxis * Group->PixelsPerMeter;
        Entry->Bitmap = Bitmap;
        Entry->Color = Color;
    }
    return Entry;
}

inline render_entry_bitmap_nearest *
PushBitmapNearest(render_group *Group, bitmap *Bitmap, v2 Origin, v2 XAxis, v2 YAxis, v4 Color)
{
    render_entry_bitmap_nearest *Entry = PushRenderElement(Group, render_entry_bitmap_nearest);
    if (Entry)
    {
        Entry->Origin = Origin * Group->PixelsPerMeter;
        Entry->XAxis = XAxis * Group->PixelsPerMeter;
        Entry->YAxis = YAxis * Group->PixelsPerMeter;
        Entry->Bitmap = Bitmap;
        Entry->Color = Color;
    }
    return Entry;
}

inline render_entry_rectangle *
PushRect(render_group *Group, v2 vMin, v2 vMax, v4 Color)
{
    render_entry_rectangle *Entry = PushRenderElement(Group, render_entry_rectangle);
    if (Entry)
    {
        Entry->Min = Group->PixelsPerMeter * vMin;
        Entry->Max = Group->PixelsPerMeter * vMax;
        Entry->Color = Color;
    }
    return Entry;
}

inline render_entry_rectangle *
PushRect(render_group *Group, rectangle2 Rect, v4 Color)
{
    render_entry_rectangle *Entry = PushRenderElement(Group, render_entry_rectangle);
    if (Entry)
    {
        Entry->Min = Group->PixelsPerMeter * Rect.Min;
        Entry->Max = Group->PixelsPerMeter * Rect.Max;
        Entry->Color = Color;
    }
    return Entry;
}

inline void
PushRectOutline(render_group *Group, v2 vMin, v2 vMax, v4 Color, f32 Thickness)
{
    v2 LeftRectangleMin = vMin - V2(Thickness, Thickness);
    v2 LeftRectangleMax = V2(vMin.x, vMax.y + Thickness);
    PushRect(Group, LeftRectangleMin, LeftRectangleMax, Color);
    
    v2 DownRectangleMin = V2(vMin.x - Thickness, vMin.y - Thickness);
    v2 DownRectangleMax = V2(vMax.x + Thickness, vMin.y);
    PushRect(Group, DownRectangleMin, DownRectangleMax, Color);
    
    v2 UpRectangleMin = V2(vMin.x - Thickness, vMax.y);
    v2 UpRectangleMax = V2(vMax.x + Thickness, vMax.y + Thickness);
    PushRect(Group, UpRectangleMin, UpRectangleMax, Color);
    
    v2 RightRectangleMin = V2(vMax.x, vMin.y - Thickness);
    v2 RightRectangleMax = V2(vMax.x + Thickness, vMax.y + Thickness);
    PushRect(Group, RightRectangleMin, RightRectangleMax, Color);
}


inline render_entry_clear *
PushClear(render_group *Group, v4 Color)
{
    render_entry_clear *Entry = PushRenderElement(Group, render_entry_clear);
    if (Entry)
    {
        Entry->Color = Color;
    }
    return Entry;
}

struct glyph
{
    bitmap Bitmap;
    s32 AdvanceWidth;
    s32 AdvanceHeight;
    s32 LeftSideBearing;
    rectangle2i Box;
};

struct font
{
    rectangle2i Box;
    f32 SF;
    s32 Ascent;
    s32 Descent;
    s32 LineGap;
    glyph Glyphs[95];
};

internal u32
CharToGlyphIndex(char C)
{
    char Result = (32 <= C && C <= 126) ? C - 32 : '?';
    return Result;
}

inline void
PushText(render_group *Group, char *String, font *Font, f32 Scale, v2 P, v4 Color)
{
    char *C = String;
    //rectangle2i FontBox = Font->Box;
    v2 CurrentP = P;
    while (*C)
    {
        u32 GlyphIndex = CharToGlyphIndex(*C);
        glyph *Glyph = Font->Glyphs + GlyphIndex;
        
        f32 GlyphWidth = Scale * (Glyph->Box.MaxX - Glyph->Box.MinX);
        f32 GlyphHeight = Scale * (Glyph->Box.MaxY - Glyph->Box.MinY);
        
        f32 GlyphAdvanceWidth = Scale * Glyph->AdvanceWidth;
        
        v2 XAxis = V2(GlyphWidth, 0);
        v2 YAxis = V2(0, GlyphHeight);
        v2 AdjustedP = CurrentP + Scale * V2i(Glyph->Box.MinX, -Glyph->Box.MaxY);
        PushBitmap(Group, &Glyph->Bitmap, AdjustedP, XAxis, YAxis, Color);
        CurrentP += Font->SF * V2(GlyphAdvanceWidth, 0);
        ++C;
    }
}

internal rectangle2
GetTextRectangle(char *String, font *Font, f32 Scale, v2 P)
{
    rectangle2 Result = InvertedInfinityRectangle();//{P, P};
    char *C = String;
    v2 CurrentP = P;
    while (*C)
    {
        u32 GlyphIndex = CharToGlyphIndex(*C);
        glyph *Glyph = Font->Glyphs + GlyphIndex;
        f32 GlyphWidth = Scale * (Glyph->Box.MaxX - Glyph->Box.MinX);
        f32 GlyphHeight = Scale * (Glyph->Box.MaxY - Glyph->Box.MinY);
        f32 GlyphAdvanceWidth = Scale * Glyph->AdvanceWidth;
        v2 AdjustedP = CurrentP + Scale *V2i(Glyph->Box.MinX, -Glyph->Box.MaxY);
        rectangle2 GlyphRectangle = {AdjustedP, AdjustedP + V2(GlyphWidth, GlyphHeight)};
        Result = RectUnion(GlyphRectangle, Result);
        CurrentP += Font->SF * V2(GlyphAdvanceWidth, 0);
        ++C;
    }
    
    return Result;
}

internal rectangle2
GetTextRectangle(string String, font *Font, f32 Scale, v2 P)
{
    rectangle2 Result = InvertedInfinityRectangle();
    char *C = String.Base;
    v2 CurrentP = P;
    for (u32 i = 0; i < String.Size; ++i)
    {
        u32 GlyphIndex = CharToGlyphIndex(*C);
        glyph *Glyph = Font->Glyphs + GlyphIndex;
        f32 GlyphWidth = Scale * (Glyph->Box.MaxX - Glyph->Box.MinX);
        f32 GlyphHeight = Scale * (Glyph->Box.MaxY - Glyph->Box.MinY);
        f32 GlyphAdvanceWidth = Scale * Glyph->AdvanceWidth;
        v2 AdjustedP = CurrentP + Scale * V2i(Glyph->Box.MinX, -Glyph->Box.MaxY);
        rectangle2 GlyphRectangle = {AdjustedP, AdjustedP + V2(GlyphWidth, GlyphHeight)};
        Result = RectUnion(GlyphRectangle, Result);
        
        CurrentP += Font->SF * V2(GlyphAdvanceWidth, 0);
        ++C;
    }
    return Result;
}

inline void
PushText(render_group *Group, string String, font *Font, f32 Scale, v2 P, v4 Color)
{
    char *C = String.Base;
    v2 CurrentP = P;
    for (u32 i = 0; i < String.Size; ++i)
    {
        u32 GlyphIndex = CharToGlyphIndex(*C);
        glyph *Glyph = Font->Glyphs + GlyphIndex;
        
        f32 GlyphWidth = Scale * (Glyph->Box.MaxX - Glyph->Box.MinX);
        f32 GlyphHeight = Scale * (Glyph->Box.MaxY - Glyph->Box.MinY);
        f32 GlyphAdvanceWidth = Scale * Glyph->AdvanceWidth;
        
        v2 XAxis = V2(GlyphWidth, 0);
        v2 YAxis = V2(0, GlyphHeight);
        v2 AdjustedP = CurrentP + Scale * V2i(Glyph->Box.MinX, -Glyph->Box.MaxY);
        PushBitmap(Group, &Glyph->Bitmap, AdjustedP, XAxis, YAxis, Color);
        CurrentP += Font->SF * V2(GlyphAdvanceWidth, 0);
        ++C;
    }
}

internal void
PushNumber(render_group *Group, u32 Number, font *Font, f32 Scale, v2 P, v4 Color)
{
    char Chars[11];
    char String[11];
    char *Dest = String;
    u32 DigitCount = 0;
    for (; DigitCount < ArrayCount(Chars) && Number; ++DigitCount)
    {
        Chars[DigitCount] = (Number % 10) + '0';
        Number /= 10;
    }
    if (DigitCount == 0)
    {
        *Dest++ = '0';
        *Dest = 0;
    }
    else
    {
        for (s32 i = DigitCount-1; i >= 0; --i)
        {
            *Dest++ = Chars[i];
        }
        *Dest = 0;
    }
    PushText(Group, String, Font, Scale, P, Color);
}
