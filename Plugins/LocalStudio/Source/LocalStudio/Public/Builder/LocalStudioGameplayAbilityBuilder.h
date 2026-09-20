// LocalStudioGameplayAbilityBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioGameplayAbilityBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "GameplayAbility"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "GASMode", "Gameplay Ability System (GAS)"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "GASHint", "Describe Gameplay Abilities, Effects, or Attribute Sets..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine Gameplay Engineer. Generate Gameplay Ability System (GAS) definitions in JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};