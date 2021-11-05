

struct row_text_data
{
    char Chars[4];
    row_text_data *Next;
};

struct measure_text_data
{
    row_text_data *FirstRow;
    measure_text_data *Next;
    measure_text_data *Previous;
};

struct chart_text_data
{
    string Headers[5];
    measure_text_data *FirstMeasure;
    chart_text_data *Next;
};

struct smfile_header
{
    string Name;
    string Value;
    smfile_header *Next;
};

struct smfile_text_data
{
    smfile_header *FirstHeader;
    chart_text_data *FirstChart;
    b32 IsValid;
};

internal string
StringBaseSize(char *Base, u32 Size)
{
    string String = {Base, Size};
    return String;
}

internal u32
GetRowCount(measure_text_data *M)
{
    row_text_data *R = M->FirstRow;
    u32 Count = 0;
    while (R)
    {
        ++Count;
        R = R->Next;
    }
    return Count;
}

internal b32
ParseChartSection(char **AtAddress, char *End, chart_text_data *Chart, memory_arena *Arena)
{
    char *At = *AtAddress;
    u32 ChartHeaderCount = 0;
    b32 DataIsValid = true;
    
    while (At < End && DataIsValid)
    {
        if (IsWhitespace(*At))
            ++At;
        else
        {
            char *HeaderStart = At;
            for (;; ++At)
            {
                if (At == End)
                {
                    DataIsValid = false;
                    break;
                }
                if (*At == ':')
                    break;
            }
            
            if (DataIsValid)
            {
                // get one of 5 headers
                Chart->Headers[ChartHeaderCount] = StringBaseSize(HeaderStart, At - HeaderStart);
                ++At;
                ++ChartHeaderCount;
                if (ChartHeaderCount == 5)
                {
                    // get measures
                    Chart->FirstMeasure = PushZeroStruct(Arena, measure_text_data);
                    measure_text_data *CurrentMeasure = Chart->FirstMeasure;
                    row_text_data *CurrentRow = 0;
                    for (; DataIsValid; ++At)
                    {
                        if (At == End)
                            DataIsValid = false;
                        else if (*At == ';')
                            break;
                        else if (*At == ',')
                        {
                            CurrentMeasure->Next = PushZeroStruct(Arena, measure_text_data);
                            CurrentMeasure->Next->Previous = CurrentMeasure;
                            CurrentMeasure = CurrentMeasure->Next;
                            CurrentRow = 0;
                        }
                        else if (*At == '/' && At < End && At[1] == '/')
                        {
                            for (;; ++At)
                            {
                                if (At == End)
                                {
                                    DataIsValid = false;
                                    break;
                                }
                                if (*At == '\n')
                                    break;
                            }
                        }
                        else if (!IsWhitespace(*At))
                        {
                            // get a row (4 chars)
                            if (At + 4 < End)
                            {
                                if (CurrentRow)
                                {
                                    CurrentRow->Next = PushZeroStruct(Arena, row_text_data);
                                    CurrentRow = CurrentRow->Next;
                                }
                                else
                                {
                                    CurrentMeasure->FirstRow = 
                                        PushZeroStruct(Arena, row_text_data);
                                    CurrentRow = CurrentMeasure->FirstRow;
                                }
                                char *Dest = CurrentRow->Chars;
                                *Dest++ = *At++;
                                *Dest++ = *At++;
                                *Dest++ = *At++;
                                *Dest = *At;
                                
                                for (;;)
                                {
                                    if (*At == '\n')
                                        break;
                                    if (*At == ',' || *At == ';')
                                    {
                                        --At;
                                        break;
                                    }
                                    ++At;
                                }
                            }
                            else
                                DataIsValid = false;
                        }
                    }
                    
                    if (DataIsValid)
                    {
                        Assert(*At == ';');
                        break;
                    }
                }
            }
        }
    }
    
    *AtAddress = At;
    return DataIsValid;
}



internal smfile_text_data
ParseSMFile(string Data, memory_arena *Arena)
{
    smfile_text_data Result = {};
    char *At = Data.Base;
    char *End = At + Data.Size;
    
    b32 DataIsValid = true;
    
    smfile_header *CurrentHeader = 0;
    chart_text_data *CurrentChart = 0; 
    
    while (At < End)
    {
        if (*At == '#')
        {
            // expecting a string that ends with a colon.
            char *FieldnameStart = At+1;
            char *FieldnameEnd = FieldnameStart;
            
            for (;; ++FieldnameEnd)
            {
                if (FieldnameEnd == End)
                {
                    DataIsValid = false;
                    break;
                }
                if (*FieldnameEnd == ':')
                    break;
            }
            
            if (DataIsValid)
            {
                string Fieldname = 
                    StringBaseSize(FieldnameStart, FieldnameEnd - FieldnameStart);
                if (StringEquals(Fieldname, "NOTES"))
                {
                    if (CurrentChart == 0)
                    {
                        CurrentChart = PushZeroStruct(Arena, chart_text_data);
                        Result.FirstChart = CurrentChart;
                    }
                    else
                    {
                        CurrentChart->Next = PushZeroStruct(Arena, chart_text_data);
                        CurrentChart = CurrentChart->Next;
                    }
                    
                    At = FieldnameEnd+1;
                    if (!ParseChartSection(&At, End, CurrentChart, Arena))
                    {
                        DataIsValid = false;
                        break;
                    }
                }
                else
                {
                    char *FieldValueStart = FieldnameEnd + 1;
                    char *FieldValueEnd = FieldValueStart;
                    
                    for (;; ++FieldValueEnd)
                    {
                        if (FieldValueEnd == End)
                        {
                            DataIsValid = false;
                            break;
                        }
                        if (*FieldValueEnd == ';')
                            break;
                    }
                    
                    if (DataIsValid)
                    {
                        string FieldValue = 
                            StringBaseSize(FieldValueStart, FieldValueEnd - FieldValueStart);
                        
                        if (CurrentHeader == 0)
                        {
                            CurrentHeader = PushZeroStruct(Arena, smfile_header);
                            Result.FirstHeader = CurrentHeader;
                        }
                        else
                        {
                            CurrentHeader->Next = PushZeroStruct(Arena, smfile_header);
                            CurrentHeader = CurrentHeader->Next;
                        }
                        CurrentHeader->Name = Fieldname;
                        CurrentHeader->Value = FieldValue;
                        
                        At = FieldValueEnd + 1;
                    }
                    else
                        break;
                }
            }
            else
                break;
        }
        else
            ++At;
    }
    
    if (CurrentHeader == 0 || CurrentChart == 0)
        DataIsValid = false;
    Result.IsValid = DataIsValid;
    return Result;
}

enum difficulty_mode
{
    DifficultyMode_Challenge,
    DifficultyMode_Hard,
    DifficultyMode_Medium,
    DifficultyMode_Easy,
    DifficultyMode_Beginner,
    DifficultyMode_Edit,
};

