// LocalStudioAudioBuilder.cpp
#include "Builder/LocalStudioAudioBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioAudioBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON with audio parameters, attenuation settings, and MetaSound graph specs."); }

bool FLocalStudioAudioBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioAudioBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("Audio plan parsed (Stub).");
    return Result;
}