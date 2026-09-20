// LocalStudioMetaHumanBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioMetaHumanBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "MetaHuman"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "MetaHumanMode", "MetaHuman & Character Setup"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "MetaHumanHint", "Describe character traits, body type, DNA, or clothing setup..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine Character Artist. Generate MetaHuman identity and assembly JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};