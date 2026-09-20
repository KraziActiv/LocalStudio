// LocalStudioNiagaraBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioNiagaraBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "Niagara"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "NiagaraMode", "VFX & Niagara Systems"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "NiagaraHint", "Describe particle effects (e.g., 'Campfire sparks with smoke and distortion')..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine VFX Artist. Generate Niagara particle system parameters in JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};