#include "Builder/LocalStudioPCGBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioPCGBuilder::GetTechnicalGuardrails() const
{
    return TEXT("{\n  \"pcg_action\": {}\n}");
}

bool FLocalStudioPCGBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    TSharedPtr<FJsonObject> RootObject;

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutError = TEXT("Failed to parse PCG JSON from LLM response.");
        return false;
    }

    OutParsedData = RootObject;
    return true;
}

FBuilderExecutionResult FLocalStudioPCGBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("PCG builder executed successfully (stub).");
    return Result;
}