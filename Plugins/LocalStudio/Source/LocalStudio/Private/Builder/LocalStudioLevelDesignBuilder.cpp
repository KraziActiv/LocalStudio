// LocalStudioLevelDesignBuilder.cpp
#include "Builder/LocalStudioLevelDesignBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioLevelDesignBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON specifying actor classes, positions, rotations, and scales."); }

bool FLocalStudioLevelDesignBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioLevelDesignBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("Level design plan parsed (Stub).");
    return Result;
}