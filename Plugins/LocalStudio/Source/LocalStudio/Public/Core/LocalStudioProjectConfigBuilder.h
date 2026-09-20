#pragma once

#include "CoreMinimal.h"
#include "ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioProjectConfigBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return FName(TEXT("ProjectConfig")); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "ConfigMode", "Project Settings & Inputs"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "ConfigHint", "Describe configuration (e.g., 'Configure Enhanced Input for gamepad and mouse')..."); }
    virtual FString GetDefaultModel() const override { return TEXT("qwen3-coder:30b"); }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine System Architect. Generate project configuration, plugin, and input mapping JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};