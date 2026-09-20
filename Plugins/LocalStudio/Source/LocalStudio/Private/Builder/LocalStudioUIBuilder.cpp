// LocalStudioUIBuilder.cpp
#include "Builder/LocalStudioUIBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioUIBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON with widget hierarchy tree, layout anchors, styles, and event bindings."); }

bool FLocalStudioUIBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioUIBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("UMG UI plan parsed (Stub).");
    return Result;
}