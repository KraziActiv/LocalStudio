// LocalStudioFoliageBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioFoliageBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "Foliage"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "FoliageMode", "Foliage Generation"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "FoliageHint", "Describe foliage setup (e.g., 'Dense pine forest with procedural rocks')..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine Environment Artist. Generate foliage spawn configuration JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};