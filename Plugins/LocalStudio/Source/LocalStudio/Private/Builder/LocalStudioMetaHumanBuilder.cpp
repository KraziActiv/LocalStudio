// LocalStudioMetaHumanBuilder.cpp
#include "Builder/LocalStudioMetaHumanBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioMetaHumanBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON with MetaHuman DNA parameters, body assembly options, and groom bindings."); }

bool FLocalStudioMetaHumanBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioMetaHumanBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("MetaHuman character plan parsed (Stub).");
    return Result;
}