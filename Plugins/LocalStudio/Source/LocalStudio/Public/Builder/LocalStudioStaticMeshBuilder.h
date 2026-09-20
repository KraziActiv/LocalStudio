// LocalStudioStaticMeshBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioStaticMeshBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "StaticMesh"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "StaticMeshMode", "Mesh & PCG Builder"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "StaticMeshHint", "Describe mesh processing or PCG graph setup..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine Technical Artist. Generate Static Mesh configuration or PCG pipeline JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;
};