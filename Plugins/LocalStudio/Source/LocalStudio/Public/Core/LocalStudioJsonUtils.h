#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class LOCALSTUDIO_API FLocalStudioJsonUtils
{
public:
    static FString CleanLlmResponse(const FString& RawResponse);
    static FString ExtractJsonSubstring(const FString& InText);
    static bool ExtractAndParseJsonObject(const FString& RawResponse, TSharedPtr<FJsonObject>& OutJsonObject, FString& OutError);
    static bool DeserializeJsonObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutJsonObject);
    static bool DeserializeJsonArray(const FString& JsonString, TArray<TSharedPtr<FJsonValue>>& OutJsonArray);
    static bool SerializeJsonObjectToString(const TSharedRef<FJsonObject>& InJsonObject, FString& OutJsonString);
    static bool SerializeJsonArrayToString(const TArray<TSharedPtr<FJsonValue>>& InJsonArray, FString& OutJsonString);

private:
    static FString SanitizeRawJsonString(const FString& InRawJson);
};