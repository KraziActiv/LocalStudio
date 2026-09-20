#include "Core/LocalStudioJsonUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FLocalStudioJsonUtils::SanitizeRawJsonString(const FString& InRawJson)
{
    FString Result;
    Result.Reserve(InRawJson.Len() + 128);

    bool bInString = false;
    bool bEscaped = false;

    for (int32 i = 0; i < InRawJson.Len(); ++i)
    {
        const TCHAR Char = InRawJson[i];

        if (bInString)
        {
            if (bEscaped)
            {
                bEscaped = false;
                Result.AppendChar(Char);
            }
            else if (Char == TEXT('\\'))
            {
                bEscaped = true;
                Result.AppendChar(Char);
            }
            else if (Char == TEXT('"'))
            {
                bInString = false;
                Result.AppendChar(Char);
            }
            else if (Char == TEXT('\n'))
            {
                Result.Append(TEXT("\\n"));
            }
            else if (Char == TEXT('\r'))
            {
                // Ignore raw carriage returns inside JSON string values
            }
            else if (Char == TEXT('\t'))
            {
                Result.Append(TEXT("\\t"));
            }
            else
            {
                Result.AppendChar(Char);
            }
        }
        else
        {
            if (Char == TEXT('"'))
            {
                bInString = true;
            }
            Result.AppendChar(Char);
        }
    }

    return Result;
}

FString FLocalStudioJsonUtils::CleanLlmResponse(const FString& RawResponse)
{
    FString Cleaned = RawResponse;

    // 1. Convert non-breaking spaces (\u00A0) and invisible Unicode spaces to standard ASCII spaces
    Cleaned.ReplaceInline(TEXT("\u00A0"), TEXT(" ")); // Non-breaking space
    Cleaned.ReplaceInline(TEXT("\uFEFF"), TEXT(""));  // Byte order mark (BOM)
    Cleaned.ReplaceInline(TEXT("\u200B"), TEXT(""));  // Zero-width space
    Cleaned.ReplaceInline(TEXT("\u2003"), TEXT(" ")); // Em space
    Cleaned.ReplaceInline(TEXT("\u2002"), TEXT(" ")); // En space

    Cleaned.ReplaceInline(TEXT("\\u0026"), TEXT("&"));
    Cleaned.ReplaceInline(TEXT("\\u003c"), TEXT("<"));
    Cleaned.ReplaceInline(TEXT("\\u003e"), TEXT(">"));

    Cleaned = Cleaned.TrimStartAndEnd();

    // 2. Extract content from generation script tags if present
    const int32 StartTagIdx = Cleaned.Find(TEXT("<generation_script>"));
    const int32 EndTagIdx = Cleaned.Find(TEXT("</generation_script>"));
    if (StartTagIdx != INDEX_NONE && EndTagIdx != INDEX_NONE && EndTagIdx > StartTagIdx)
    {
        const int32 ScriptStart = StartTagIdx + 19;
        Cleaned = Cleaned.Mid(ScriptStart, EndTagIdx - ScriptStart).TrimStartAndEnd();
    }

    // 3. Strip Markdown code fences
    if (Cleaned.StartsWith(TEXT("```json")))
    {
        Cleaned.RemoveFromStart(TEXT("```json"));
    }
    else if (Cleaned.StartsWith(TEXT("```")))
    {
        Cleaned.RemoveFromStart(TEXT("```"));
    }

    if (Cleaned.EndsWith(TEXT("```")))
    {
        Cleaned.RemoveFromEnd(TEXT("```"));
    }

    Cleaned = Cleaned.TrimStartAndEnd();

    // 4. Extract JSON object boundaries
    int32 FirstBrace = Cleaned.Find(TEXT("{\"action\""));
    if (FirstBrace == INDEX_NONE)
    {
        FirstBrace = Cleaned.Find(TEXT("{"));
    }

    int32 LastBrace = Cleaned.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
    if (FirstBrace != INDEX_NONE && LastBrace != INDEX_NONE && LastBrace > FirstBrace)
    {
        Cleaned = Cleaned.Mid(FirstBrace, (LastBrace - FirstBrace) + 1);
    }

    // 5. Sanitize invalid ASCII control characters without double-escaping control sequences
    FString SanitizedResult;
    SanitizedResult.Reserve(Cleaned.Len());

    for (int32 Index = 0; Index < Cleaned.Len(); ++Index)
    {
        TCHAR Char = Cleaned[Index];

        if (Char == '\r')
        {
            // Skip carriage returns
            continue;
        }
        else if (Char < 32 && Char != '\n' && Char != '\t')
        {
            // Skip invalid ASCII control characters below 32 except newline and tab
            continue;
        }

        SanitizedResult.AppendChar(Char);
    }

    return SanitizedResult;
}

FString FLocalStudioJsonUtils::ExtractJsonSubstring(const FString& InText)
{
    int32 FirstBrace = InText.Find(TEXT("{"));
    if (FirstBrace != INDEX_NONE)
    {
        return InText.Mid(FirstBrace);
    }
    return InText;
}

bool FLocalStudioJsonUtils::ExtractAndParseJsonObject(const FString& RawResponse, TSharedPtr<FJsonObject>& OutJsonObject, FString& OutError)
{
    FString CleanedText = CleanLlmResponse(RawResponse);

    // 1. Try parsing raw cleaned string directly
    if (DeserializeJsonObject(CleanedText, OutJsonObject) && OutJsonObject.IsValid())
    {
        return true;
    }

    // 2. Fallback to sanitizing raw unescaped string literals and control characters
    FString SanitizedText = SanitizeRawJsonString(CleanedText);
    if (DeserializeJsonObject(SanitizedText, OutJsonObject) && OutJsonObject.IsValid())
    {
        return true;
    }

    OutError = FString::Printf(TEXT("Failed to parse valid JSON structure from AI output. Payload length: %d"), CleanedText.Len());
    return false;
}

bool FLocalStudioJsonUtils::DeserializeJsonObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutJsonObject)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    return FJsonSerializer::Deserialize(Reader, OutJsonObject) && OutJsonObject.IsValid();
}

bool FLocalStudioJsonUtils::DeserializeJsonArray(const FString& JsonString, TArray<TSharedPtr<FJsonValue>>& OutJsonArray)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    return FJsonSerializer::Deserialize(Reader, OutJsonArray);
}

bool FLocalStudioJsonUtils::SerializeJsonObjectToString(const TSharedRef<FJsonObject>& InJsonObject, FString& OutJsonString)
{
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
    return FJsonSerializer::Serialize(InJsonObject, Writer);
}

bool FLocalStudioJsonUtils::SerializeJsonArrayToString(const TArray<TSharedPtr<FJsonValue>>& InJsonArray, FString& OutJsonString)
{
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
    return FJsonSerializer::Serialize(InJsonArray, Writer);
}