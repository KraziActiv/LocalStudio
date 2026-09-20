#include "Core/LocalStudioProjectConfigBuilder.h"
#include "Core/LocalStudioJsonUtils.h"

FString FLocalStudioProjectConfigBuilder::GetTechnicalGuardrails() const
{
    return TEXT("Output valid JSON targeting DefaultEngine.ini settings, Input Actions, and Mapping Contexts.");
}

bool FLocalStudioProjectConfigBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    // Replaces all 30+ lines of duplicate stripping, string manipulation, and reader boilerplate!
    return FLocalStudioJsonUtils::ExtractAndParseJsonObject(RawResponse, OutParsedData, OutError);
}

FBuilderExecutionResult FLocalStudioProjectConfigBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;

    if (!ParsedData.IsValid())
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Invalid JSON data provided to ExecutePlan.");
        return Result;
    }

    Result.bSuccess = true;
    Result.UserSummary = TEXT("Project configuration plan parsed successfully.");
    return Result;
}