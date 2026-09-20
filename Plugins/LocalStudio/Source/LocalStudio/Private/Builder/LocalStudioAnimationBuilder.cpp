// LocalStudioAnimationBuilder.cpp
#include "Builder/LocalStudioAnimationBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioAnimationBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON specifying animation state machines, blend parameters, and slots."); }

bool FLocalStudioAnimationBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioAnimationBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("Animation system plan parsed (Stub).");
    return Result;
}