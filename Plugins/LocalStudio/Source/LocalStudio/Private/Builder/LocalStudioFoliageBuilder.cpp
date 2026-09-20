// LocalStudioFoliageBuilder.cpp
#include "Builder/LocalStudioFoliageBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioFoliageBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON with foliage density, mesh asset paths, and scale transforms."); }

bool FLocalStudioFoliageBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioFoliageBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("Foliage configuration parsed (Stub).");
    return Result;
}