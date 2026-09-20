#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FBuilderExecutionResult
{
    bool bSuccess = false;
    FString UserSummary;
    FString ErrorMessage;
};

class LOCALSTUDIO_API ILocalStudioBuilder
{
public:
    virtual ~ILocalStudioBuilder() = default;

    virtual FName GetBuilderID() const = 0;
    virtual FText GetDisplayName() const = 0;
    virtual FString GetDefaultModel() const = 0;
    virtual FString GetDefaultJobDescription() const = 0;
    virtual FString GetTechnicalGuardrails() const = 0;

    /** Returns the placeholder/hint text displayed in the Slate input text box for this builder. */
    virtual FText GetInputHintText() const
    {
        return NSLOCTEXT("LocalStudio", "DefaultHint", "Type instructions or special actions here...");
    }

    virtual FString PreparePromptContext(const FString& RawUserPrompt)
    {
        return RawUserPrompt;
    }

    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) = 0;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) = 0;
};