#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Core/StructEnums.h"
#include "Widgets/SBoxPanel.h"

// Forward declarations to minimize header dependencies
class UOllamaManager;
class SMultiLineEditableTextBox;
class SScrollBox;
class SButton;
class SImage;
class ILocalStudioBuilder;
class FJsonObject;

template <typename OptionType>
class SComboBox;

class LOCALSTUDIO_API SLocalStudioChatPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLocalStudioChatPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TWeakObjectPtr<UOllamaManager> InManager);

private:
    FText GetChatHistoryText() const;
    void OnInputTextChanged(const FText& NewText);
    FReply OnBuildProjectClicked();
    void AppendToConsoleLog(const FString& NewMessage);
    void OnGenerationStreamReceived(const FString& FinalResponse, bool bSuccess);
    bool CanExecutePlan() const;
    FReply OnExecutePlanClicked();

    // Builder & Mode Management
    void InitializeModeTemplates();
    void OnModeSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    FText GetJobDescriptionText() const;
    void OnJobDescriptionChanged(const FText& NewText);

    FReply OnClearThreadClicked();

    // Current Builder Execution State
    FName ActiveBuilderID;
    TSharedPtr<ILocalStudioBuilder> CurrentBuilder;
    TSharedPtr<FJsonObject> CachedParsedData;

private:
    TWeakObjectPtr<UOllamaManager> OllamaManagerPtr;
    FText ChatHistoryLog;

    TSharedPtr<SVerticalBox> ChatMessagesBox;

    void AddChatMessageWidget(const FString& Message);
    void RebuildChatMessageWidgets();

    FText ActiveInputText;
    FString PendingPlanJson;

    // UI Element References
    TSharedPtr<SMultiLineEditableTextBox> InputTextBox;
    TSharedPtr<SScrollBox> ChatScrollBox;
    TSharedPtr<SButton> ExecutePlanButton;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ModeComboBox;
    TSharedPtr<SMultiLineEditableTextBox> JobDescriptionTextBox;

    // Mode Options & Overrides
    TArray<TSharedPtr<FString>> ModeOptions;
    TMap<FName, FString> ModeJobDescriptions;

private:
    void RefreshPanelToCurrentSettings();
    bool bIgnoreOnTextChanged = false;

    // Confirms overwrite/rename policy if target asset already exists
    EAssetConflictResolution PromptAssetConflictDialog(const FString& ExistingAssetName, FString& OutNewName);

    // Helper to execute plan with chosen overwrite policy
    void RunExecutionWithPolicy(bool bAllowOverwrite, const FString& TargetNameOverride);

    FReply OnCancelScriptChangesClicked();

};