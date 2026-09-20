// LocalStudioGameplayAbilityBuilder.cpp
#include "Builder/LocalStudioGameplayAbilityBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FLocalStudioGameplayAbilityBuilder::GetTechnicalGuardrails() const { return TEXT("Output valid JSON defining Gameplay Attributes, Ability Tasks, Gameplay Tags, and Effects."); }

bool FLocalStudioGameplayAbilityBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResponse);
    return FJsonSerializer::Deserialize(Reader, OutParsedData) && OutParsedData.IsValid();
}

FBuilderExecutionResult FLocalStudioGameplayAbilityBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;
    Result.UserSummary = TEXT("Gameplay Ability System plan parsed (Stub).");
    return Result;
}