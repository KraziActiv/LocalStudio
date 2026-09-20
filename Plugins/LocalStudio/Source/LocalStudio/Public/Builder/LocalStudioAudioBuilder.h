// LocalStudioAudioBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioAudioBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "Audio"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "AudioMode", "MetaSounds & Audio"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "AudioHint", "Describe audio setup, MetaSounds graph, or spatialization..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine Sound Designer. Generate MetaSounds and Sound Cue configurations in JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};