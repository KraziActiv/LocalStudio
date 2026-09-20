// LocalStudioUIBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioUIBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "UI"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "UIMode", "UMG & UI Builder"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "UIHint", "Describe user interface (e.g., 'Health bar HUD with inventory grid')..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine UI Designer. Generate UMG widget hierarchies and data bindings in JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};