// LocalStudioStaticMeshBuilder.cpp
#include "Builder/LocalStudioStaticMeshBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioStaticMeshBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON specifying static mesh import/generation properties or PCG node graphs."); }

bool FLocalStudioStaticMeshBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioStaticMeshBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("Static Mesh plan parsed (Stub).");
    return Result;
}