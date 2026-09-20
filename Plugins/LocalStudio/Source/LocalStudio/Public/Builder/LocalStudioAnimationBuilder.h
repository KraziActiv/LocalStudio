// LocalStudioAnimationBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioAnimationBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "Animation"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "AnimationMode", "Animation & AnimBP"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "AnimationHint", "Describe Anim Blueprint logic, blendspaces, or montages..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine Animator. Generate Animation Blueprint and State Machine specifications in JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};