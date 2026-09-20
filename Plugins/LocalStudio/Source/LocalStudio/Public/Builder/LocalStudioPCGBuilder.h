#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioPCGBuilder : public ILocalStudioBuilder
{
public:
    // --- ILocalStudioBuilder Interface ---
    virtual FName GetBuilderID() const override { return "PCG"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "PCGMode", "PCG Generation"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "PCGHint", "Describe PCG rules or scattering..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override
    {
        return TEXT("You are an Unreal Engine PCG Assistant. Generate JSON PCG specifications.");
    }

    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};