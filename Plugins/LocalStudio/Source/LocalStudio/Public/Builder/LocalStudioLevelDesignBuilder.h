// LocalStudioLevelDesignBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioLevelDesignBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "LevelDesign"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "LevelDesignMode", "Level & Actor Layout"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "LevelDesignHint", "Describe room blockout, lighting setups, or actor placements..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine Level Designer. Generate level layout and actor placement JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};