// LocalStudioNiagaraBuilder.cpp
#include "Builder/LocalStudioNiagaraBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioNiagaraBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON defining Niagara system parameters, emitters, and user variables."); }

bool FLocalStudioNiagaraBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioNiagaraBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("Niagara VFX plan parsed (Stub).");
    return Result;
}