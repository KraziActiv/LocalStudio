#include "Builder/GeneralChatBuilder.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

FString FGeneralChatBuilder::GetDefaultJobDescription() const
{
    return TEXT("You are a helpful, witty, and grounded Unreal Engine AI assistant companion. "
        "Answer questions directly, assist with Blueprint/C++ code troubleshooting, "
        "and offer actionable project advice without modifying assets or files.");
}

FString FGeneralChatBuilder::GetTechnicalGuardrails() const
{
    return TEXT("Do NOT attempt to format output as a JSON action plan or execute asset changes. "
        "Respond in direct conversational text.");
}

bool FGeneralChatBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    OutParsedData = MakeShared<FJsonObject>();
    OutParsedData->SetStringField(TEXT("reply"), RawResponse);
    return true;
}

FBuilderExecutionResult FGeneralChatBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = true;

    if (ParsedData.IsValid() && ParsedData->HasField(TEXT("reply")))
    {
        Result.UserSummary = ParsedData->GetStringField(TEXT("reply"));
    }
    else
    {
        Result.UserSummary = TEXT("No response text provided.");
    }

    return Result;
}

FString FGeneralChatBuilder::GatherRecentProjectLogs(int32 MaxLines)
{
    FString LogFilePath = FPaths::ProjectLogDir() / FApp::GetProjectName() + TEXT(".log");
    TArray<FString> LogLines;

    if (FFileHelper::LoadFileToStringArray(LogLines, *LogFilePath))
    {
        int32 StartIndex = FMath::Max(0, LogLines.Num() - MaxLines);
        FString OutputLog = TEXT("\n--- RECENT PROJECT LOG DIAGNOSTICS ---\n");
        for (int32 i = StartIndex; i < LogLines.Num(); ++i)
        {
            OutputLog += LogLines[i] + TEXT("\n");
        }
        return OutputLog;
    }

    return TEXT("");
}

FString FGeneralChatBuilder::GatherProjectDirectoryStructure()
{
    FString PluginSourceDir = FPaths::ProjectPluginsDir() / TEXT("LocalStudio/Source/LocalStudio/");
    TArray<FString> FoundFiles;

    IFileManager::Get().FindFilesRecursive(FoundFiles, *PluginSourceDir, TEXT("*.*"), true, false);

    FString StructureSummary = TEXT("\n--- LOCALSTUDIO PLUGIN SOURCE STRUCTURE ---\n");
    for (const FString& FilePath : FoundFiles)
    {
        FString RelativePath = FilePath;
        FPaths::MakePathRelativeTo(RelativePath, *FPaths::ProjectDir());
        StructureSummary += FString::Printf(TEXT("- %s\n"), *RelativePath);
    }

    return StructureSummary;
}

FString FGeneralChatBuilder::ReadSpecificFile(const FString& RelativeFilePath)
{
    FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativeFilePath);
    FString Content;
    if (FFileHelper::LoadFileToString(Content, *FullPath))
    {
        return FString::Printf(TEXT("\n--- FILE: %s ---\n%s\n"), *RelativeFilePath, *Content);
    }
    return FString::Printf(TEXT("\n--- Could not locate file: %s ---\n"), *RelativeFilePath);
}

FString FGeneralChatBuilder::PreparePromptContext(const FString& RawUserPrompt)
{
    FString ProjectContext = GatherProjectDirectoryStructure();
    FString RecentLogs = GatherRecentProjectLogs(30);

    return FString::Printf(
        TEXT("%s\n\n[PROJECT CONTEXT]\n%s\n%s"),
        *RawUserPrompt,
        *ProjectContext,
        *RecentLogs
    );
}