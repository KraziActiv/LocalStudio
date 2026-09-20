#pragma once

#include "CoreMinimal.h"
#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FGeneralChatBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "GeneralChat"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "GeneralChatMode", "General Chat (Conversational)"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "GeneralChatHint", "Type casual chat or ask Unreal Engine questions here..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override;
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;

    // Read-only Project Context Helpers for Troubleshooting
    static FString GatherRecentProjectLogs(int32 MaxLines = 50);
    static FString GatherProjectDirectoryStructure();
    static FString ReadSpecificFile(const FString& RelativeFilePath);
    virtual FString PreparePromptContext(const FString& RawUserPrompt);
};