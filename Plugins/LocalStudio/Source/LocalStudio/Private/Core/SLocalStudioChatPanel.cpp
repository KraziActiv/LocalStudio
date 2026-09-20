#include "Core/SLocalStudioChatPanel.h"
#include "Core/OllamaManager.h"
#include "Core/LocalStudioSettings.h"
#include "Core/LocalStudioAssetUtils.h"
#include "Core/ChatHistoryManager.h"
#include "Core/BuilderRegistry.h"
#include "Core/LocalStudioJsonUtils.h"

#include "Editor.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Styling/AppStyle.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"

void SLocalStudioChatPanel::Construct(const FArguments& InArgs, TWeakObjectPtr<UOllamaManager> InManager)
{
    OllamaManagerPtr = InManager;

    InitializeModeTemplates();

    FString AccumulatedHistoryText = TEXT("Local Studio Operational.\n");
    if (GEditor)
    {
        if (UChatHistoryManager* HistoryMgr = GEditor->GetEditorSubsystem<UChatHistoryManager>())
        {
            const TArray<FChatMessage>& SavedHistory = HistoryMgr->GetChatHistory();
            for (const FChatMessage& Msg : SavedHistory)
            {
                AccumulatedHistoryText += FString::Printf(TEXT("\n\n%s: %s"), *Msg.Sender, *Msg.Message);
            }
        }
    }

    ChatHistoryLog = FText::FromString(AccumulatedHistoryText);

    ChildSlot
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.0f)
                [
                    SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
                        .Padding(6.0f)
                        [
                            SNew(SVerticalBox)

                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 0, 0, 4.0f)
                                [
                                    SNew(SHorizontalBox)
                                        + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        .Padding(0, 0, 8.0f, 0)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("Specialization Mode:")))
                                                .Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
                                        ]
                                        + SHorizontalBox::Slot()
                                        .FillWidth(1.0f)
                                        [
                                            SAssignNew(ModeComboBox, SComboBox<TSharedPtr<FString>>)
                                                .OptionsSource(&ModeOptions)
                                                .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                                                return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : TEXT("")));
                                                    })
                                                .OnSelectionChanged(this, &SLocalStudioChatPanel::OnModeSelectionChanged)
                                                [
                                                    SNew(STextBlock).Text_Lambda([this]() {
                                                        TSharedPtr<ILocalStudioBuilder> Builder = FLocalStudioBuilderRegistry::Get().GetBuilder(ActiveBuilderID);
                                                        return Builder.IsValid() ? Builder->GetDisplayName() : FText::FromString(TEXT("General Chat"));
                                                        })
                                                ]
                                        ]
                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .Padding(6.0f, 0, 0, 0)
                                        .VAlign(VAlign_Center)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                                .OnClicked(this, &SLocalStudioChatPanel::OnClearThreadClicked)
                                                .ToolTipText(FText::FromString(TEXT("Wipes the active session memory context and resets logs.")))
                                                [
                                                    SNew(SHorizontalBox)
                                                        + SHorizontalBox::Slot()
                                                        .AutoWidth()
                                                        .VAlign(VAlign_Center)
                                                        [
                                                            SNew(SImage)
                                                                .Image(FAppStyle::GetBrush("Icons.Refresh"))
                                                        ]
                                                        + SHorizontalBox::Slot()
                                                        .AutoWidth()
                                                        .Padding(4.0f, 0, 0, 0)
                                                        .VAlign(VAlign_Center)
                                                        [
                                                            SNew(STextBlock).Text(FText::FromString(TEXT("Restart Session")))
                                                        ]
                                                ]
                                        ]
                                ]

                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Active System Instruction Prompt (Editable):")))
                                        .Font(FAppStyle::Get().GetFontStyle("NormalFontSubdued"))
                                ]
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                                [
                                    SAssignNew(JobDescriptionTextBox, SMultiLineEditableTextBox)
                                        .Text(this, &SLocalStudioChatPanel::GetJobDescriptionText)
                                        .OnTextChanged(this, &SLocalStudioChatPanel::OnJobDescriptionChanged)
                                        .AutoWrapText(true)
                                ]
                        ]
                ]

            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(8.0f)
                [
                    SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush("Menu.Background"))
                        .Padding(6.0f)
                        [
                            SAssignNew(ChatScrollBox, SScrollBox)
                                + SScrollBox::Slot()
                                [
                                    SAssignNew(ChatMessagesBox, SVerticalBox)
                                ]
                        ]
                ]

            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 4)
                [
                    SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush("Menu.Separator"))
                        .ContentScale(FVector2D(1.0f, 2.0f))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.0f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(0, 0, 8.0f, 0)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(InputTextBox, SMultiLineEditableTextBox)
                                .HintText(FText::FromString(TEXT("Enter your prompt instructions here...")))
                                .OnTextChanged(this, &SLocalStudioChatPanel::OnInputTextChanged)
                                .AutoWrapText(true)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                                .ContentPadding(FMargin(16, 8))
                                .OnClicked(this, &SLocalStudioChatPanel::OnBuildProjectClicked)
                                [
                                    SNew(STextBlock).Text(FText::FromString(TEXT("Send Directive")))
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.0f, 0, 0, 0)
                        [
                            SAssignNew(ExecutePlanButton, SButton)
                                .ContentPadding(FMargin(16, 8))
                                .IsEnabled(this, &SLocalStudioChatPanel::CanExecutePlan)
                                .OnClicked(this, &SLocalStudioChatPanel::OnExecutePlanClicked)
                                [
                                    SNew(STextBlock).Text(FText::FromString(TEXT("Apply Script Changes")))
                                ]
                        ]

                    // --- CANCEL BUTTON ---
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.0f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Discard Plan")))
                                .OnClicked(this, &SLocalStudioChatPanel::OnCancelScriptChangesClicked)
                                .IsEnabled_Lambda([this]() { return CachedParsedData.IsValid(); }) 
                        ]

                ]
        ];

        AddChatMessageWidget(TEXT("Local Studio Operational."));

        if (GEditor)
        {
            if (UChatHistoryManager* HistoryMgr =
                GEditor->GetEditorSubsystem<UChatHistoryManager>())
            {
                const TArray<FChatMessage>& SavedHistory =
                    HistoryMgr->GetChatHistory();

                for (const FChatMessage& Msg : SavedHistory)
                {
                    AddChatMessageWidget(
                        FString::Printf(
                            TEXT("%s: %s"),
                            *Msg.Sender,
                            *Msg.Message
                        )
                    );
                }
            }
        }

    if (ULocalStudioSettings* Settings = GetMutableDefault<ULocalStudioSettings>())
    {
        Settings->OnSettingsChanged.AddRaw(this, &SLocalStudioChatPanel::RefreshPanelToCurrentSettings);
    }
}

FReply SLocalStudioChatPanel::OnClearThreadClicked()
{
    ActiveInputText = FText::GetEmpty();
    if (InputTextBox.IsValid())
    {
        InputTextBox->SetText(FText::GetEmpty());
    }

    ChatHistoryLog = FText::FromString(
        TEXT("Local Studio Operational Context Cleared.\nSelect a specialization mode or edit baseline rules above...")
    );

    if (ChatMessagesBox.IsValid())
    {
        ChatMessagesBox->ClearChildren();

        AddChatMessageWidget(
            TEXT("Local Studio Operational Context Cleared.")
        );

        AddChatMessageWidget(
            TEXT("System: Select a specialization mode or edit baseline rules above...")
        );
    }

    if (GEditor)
    {
        if (UChatHistoryManager* HistoryMgr = GEditor->GetEditorSubsystem<UChatHistoryManager>())
        {
            HistoryMgr->ClearHistory();
        }
    }

    if (OllamaManagerPtr.IsValid())
    {
        OllamaManagerPtr->ClearActiveContext();
    }

    if (ChatScrollBox.IsValid())
    {
        ChatScrollBox->ScrollToStart();
    }

    return FReply::Handled();
}

void SLocalStudioChatPanel::RefreshPanelToCurrentSettings()
{
    if (JobDescriptionTextBox.IsValid())
    {
        bIgnoreOnTextChanged = true;
        JobDescriptionTextBox->SetText(GetJobDescriptionText());
        bIgnoreOnTextChanged = false;
    }
}

void SLocalStudioChatPanel::InitializeModeTemplates()
{
    ModeOptions.Empty();

    const auto& RegisteredBuilders = FLocalStudioBuilderRegistry::Get().GetAllBuilders();
    for (const auto& Pair : RegisteredBuilders)
    {
        ModeOptions.Add(MakeShared<FString>(Pair.Value->GetDisplayName().ToString()));
    }

    if (RegisteredBuilders.Contains("GeneralChat"))
    {
        ActiveBuilderID = "GeneralChat";
    }
    else if (RegisteredBuilders.Num() > 0)
    {
        ActiveBuilderID = RegisteredBuilders.CreateConstIterator()->Key;
    }
}

void SLocalStudioChatPanel::OnModeSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (!NewSelection.IsValid()) return;

    const auto& RegisteredBuilders = FLocalStudioBuilderRegistry::Get().GetAllBuilders();
    for (const auto& Pair : RegisteredBuilders)
    {
        if (Pair.Value->GetDisplayName().ToString().Equals(*NewSelection))
        {
            ActiveBuilderID = Pair.Key;
            break;
        }
    }

    if (JobDescriptionTextBox.IsValid())
    {
        JobDescriptionTextBox->SetText(GetJobDescriptionText());
    }
}

FText SLocalStudioChatPanel::GetJobDescriptionText() const
{
    TSharedPtr<ILocalStudioBuilder> Builder = FLocalStudioBuilderRegistry::Get().GetBuilder(ActiveBuilderID);
    if (Builder.IsValid())
    {
        return FText::FromString(Builder->GetDefaultJobDescription());
    }
    return FText::GetEmpty();
}

void SLocalStudioChatPanel::OnJobDescriptionChanged(const FText& NewText)
{
    if (bIgnoreOnTextChanged) return;

    ULocalStudioSettings* Settings = GetMutableDefault<ULocalStudioSettings>();
    if (Settings)
    {
        FString ProfileKey = ActiveBuilderID.ToString();
        if (FOllamaTaskProfile* Profile = Settings->TaskProfiles.Find(ProfileKey))
        {
            Profile->JobDescription = NewText.ToString();
            Settings->TryUpdateDefaultConfigFile(Settings->GetDefaultConfigFilename());
        }
    }
}

FText SLocalStudioChatPanel::GetChatHistoryText() const
{
    return ChatHistoryLog;
}

void SLocalStudioChatPanel::AddChatMessageWidget(const FString& Message)
{
    if (!ChatMessagesBox.IsValid())
    {
        return;
    }

    FString DisplayMessage = Message;
    DisplayMessage.TrimStartAndEndInline();

    FLinearColor TextColor = FLinearColor::White;

    // User messages
    if (DisplayMessage.StartsWith(TEXT("User:"), ESearchCase::IgnoreCase))
    {
        TextColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
    }
    // System errors
    else if (DisplayMessage.StartsWith(TEXT("System Error:"), ESearchCase::IgnoreCase))
    {
        TextColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
    }
    // System warnings
    else if (DisplayMessage.StartsWith(TEXT("System Warning:"), ESearchCase::IgnoreCase))
    {
        TextColor = FLinearColor(1.0f, 0.75f, 0.0f, 1.0f);
    }
    // Successful operations
    else if (DisplayMessage.StartsWith(TEXT("System Success:"), ESearchCase::IgnoreCase))
    {
        TextColor = FLinearColor(0.0f, 1.0f, 0.4f, 1.0f);
    }
    // Normal system messages
    else if (DisplayMessage.StartsWith(TEXT("System:"), ESearchCase::IgnoreCase))
    {
        TextColor = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("LocalStudio Chat UI: [%s] -> Color R=%f G=%f B=%f"),
        *DisplayMessage.Left(80),
        TextColor.R,
        TextColor.G,
        TextColor.B
    );

    ChatMessagesBox->AddSlot()
        .AutoHeight()
        .Padding(4.0f, 2.0f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(DisplayMessage))
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor(TextColor))
                .Font(FAppStyle::Get().GetFontStyle("NormalFont"))
        ];
}

void SLocalStudioChatPanel::OnInputTextChanged(const FText& NewText)
{
    ActiveInputText = NewText;
}

FReply SLocalStudioChatPanel::OnBuildProjectClicked()
{
    FString UserPrompt = ActiveInputText.ToString().TrimStartAndEnd();
    if (UserPrompt.IsEmpty())
    {
        return FReply::Handled();
    }

    if (!OllamaManagerPtr.IsValid())
    {
        AppendToConsoleLog(TEXT("\n\nSystem Error: Ollama Manager link unreachable."));
        return FReply::Handled();
    }

    CachedParsedData.Reset();
    AppendToConsoleLog(FString::Printf(TEXT("\n\nUser: %s"), *UserPrompt));

    if (GEditor)
    {
        if (UChatHistoryManager* HistoryMgr = GEditor->GetEditorSubsystem<UChatHistoryManager>())
        {
            HistoryMgr->AddChatMessage(TEXT("User"), UserPrompt);
        }
    }

    if (InputTextBox.IsValid())
    {
        InputTextBox->SetText(FText::GetEmpty());
    }
    ActiveInputText = FText::GetEmpty();

    AppendToConsoleLog(TEXT("\n\nSystem: Processing instructions..."));

    TSharedPtr<ILocalStudioBuilder> ActiveBuilder = FLocalStudioBuilderRegistry::Get().GetBuilder(ActiveBuilderID);

    FString FinalPrompt = ActiveBuilder.IsValid()
        ? ActiveBuilder->PreparePromptContext(UserPrompt)
        : UserPrompt;

    FString ActiveProfileName = ActiveBuilderID.ToString();

    TWeakPtr<SLocalStudioChatPanel> WeakSelf = SharedThis(this);

    OllamaManagerPtr->RequestReasoning(FinalPrompt, ActiveProfileName, [WeakSelf](const FString& FinalResponse, bool bSuccess)
        {
            if (TSharedPtr<SLocalStudioChatPanel> SafeSelf = WeakSelf.Pin())
            {
                SafeSelf->OnGenerationStreamReceived(FinalResponse, bSuccess);
            }
        });

    return FReply::Handled();
}

void SLocalStudioChatPanel::AppendToConsoleLog(const FString& NewMessage)
{
    ChatHistoryLog = FText::FromString(
        ChatHistoryLog.ToString() + NewMessage
    );

    AddChatMessageWidget(NewMessage);

    if (ChatScrollBox.IsValid())
    {
        ChatScrollBox->ScrollToEnd();
    }
}

void SLocalStudioChatPanel::OnGenerationStreamReceived(const FString& FinalResponse, bool bSuccess)
{
    if (!bSuccess)
    {
        if (!bSuccess)
        {
            AsyncTask(ENamedThreads::GameThread, [this, FinalResponse]()
                {
                    // If FinalResponse contains detailed error message from OllamaManager, display it
                    FString DisplayError = FinalResponse.IsEmpty()
                        ? TEXT("System Error: Request failed or server unreachable.")
                        : FString::Printf(TEXT("\n\nSystem Error: %s"), *FinalResponse);

                    AppendToConsoleLog(DisplayError);
                });
            return;
        }
    }

    AsyncTask(ENamedThreads::GameThread, [this, FinalResponse]()
        {
            TSharedPtr<ILocalStudioBuilder> Builder = FLocalStudioBuilderRegistry::Get().GetBuilder(ActiveBuilderID);
            if (!Builder.IsValid())
            {
                AppendToConsoleLog(TEXT("\n\nSystem Error: Selected builder is invalid or not registered."));
                return;
            }

            FString CleanedResponse = FLocalStudioJsonUtils::CleanLlmResponse(FinalResponse);

            TSharedPtr<FJsonObject> ParsedData;
            FString Error;

            if (Builder->ParseResponse(CleanedResponse, ParsedData, Error))
            {
                if (ActiveBuilderID == "GeneralChat")
                {
                    FBuilderExecutionResult Result = Builder->ExecutePlan(ParsedData);
                    AppendToConsoleLog(FString::Printf(TEXT("\n\nSystem: %s"), *Result.UserSummary));

                    if (GEditor)
                    {
                        if (UChatHistoryManager* HistoryMgr = GEditor->GetEditorSubsystem<UChatHistoryManager>())
                        {
                            HistoryMgr->AddChatMessage(TEXT("System"), Result.UserSummary);
                        }
                    }
                    CachedParsedData.Reset();
                }
                else
                {
                    CachedParsedData = ParsedData;
                    AppendToConsoleLog(FString::Printf(
                        TEXT("\n\nSystem: Script plan generated for [%s]. Review details and click 'Apply Script Changes'."),
                        *ActiveBuilderID.ToString()
                    ));
                }
            }
            else
            {
                CachedParsedData.Reset();
                AppendToConsoleLog(FString::Printf(TEXT("\n\nSystem Error: %s"), *Error));
            }
        });
}

bool SLocalStudioChatPanel::CanExecutePlan() const
{
    return CachedParsedData.IsValid();
}

FReply SLocalStudioChatPanel::OnExecutePlanClicked()
{
    if (!CachedParsedData.IsValid()) return FReply::Handled();

    FString TargetAssetName = CachedParsedData->HasTypedField<EJson::String>(TEXT("name"))
        ? CachedParsedData->GetStringField(TEXT("name")) : TEXT("M_GeneratedMaterial");

    UObject* ExistingAsset = FLocalStudioAssetUtils::FindAssetByName<UObject>(TargetAssetName);

    if (ExistingAsset)
    {
        FString NewNameProposal = TargetAssetName;
        EAssetConflictResolution Choice = PromptAssetConflictDialog(TargetAssetName, NewNameProposal);

        if (Choice == EAssetConflictResolution::Cancel)
        {
            AppendToConsoleLog(TEXT("\n\nSystem: Operation cancelled by user."));
            return FReply::Handled();
        }

        if (Choice == EAssetConflictResolution::SaveAsNew)
        {
            CachedParsedData->SetStringField(TEXT("name"), NewNameProposal);
            RunExecutionWithPolicy(false, NewNameProposal);
            return FReply::Handled();
        }

        if (Choice == EAssetConflictResolution::Overwrite)
        {
            RunExecutionWithPolicy(true, TargetAssetName);
            return FReply::Handled();
        }
    }

    RunExecutionWithPolicy(true, TargetAssetName);
    return FReply::Handled();
}

void SLocalStudioChatPanel::RunExecutionWithPolicy(
    bool bAllowOverwrite,
    const FString& TargetNameOverride)
{
    TSharedPtr<ILocalStudioBuilder> Builder =
        FLocalStudioBuilderRegistry::Get().GetBuilder(ActiveBuilderID);

    if (!Builder.IsValid())
    {
        AppendToConsoleLog(
            TEXT("\n\nSystem Error: Selected builder is invalid or not registered.")
        );

        CachedParsedData.Reset();
        return;
    }

    if (!CachedParsedData.IsValid())
    {
        AppendToConsoleLog(
            TEXT("\n\nSystem Error: No cached execution plan is available.")
        );

        return;
    }

    CachedParsedData->SetBoolField(
        TEXT("allow_overwrite"),
        bAllowOverwrite
    );

    FBuilderExecutionResult Result =
        Builder->ExecutePlan(CachedParsedData);

    if (Result.bSuccess)
    {
        AppendToConsoleLog(FString::Printf(
            TEXT("\n\nSystem Success: %s"),
            *Result.UserSummary
        ));
    }
    else
    {
        FString ErrorText;

        if (!Result.UserSummary.IsEmpty())
        {
            ErrorText = Result.UserSummary;
        }
        else if (!Result.ErrorMessage.IsEmpty())
        {
            ErrorText = Result.ErrorMessage;
        }
        else
        {
            ErrorText =
                TEXT("The builder failed without providing an error message.");
        }

        UE_LOG(
            LogTemp,
            Error,
            TEXT("LocalStudio execution failed: %s"),
            *ErrorText
        );

        AppendToConsoleLog(FString::Printf(
            TEXT("\n\nSystem Error: %s"),
            *ErrorText
        ));
    }

    CachedParsedData.Reset();
}

FReply SLocalStudioChatPanel::OnCancelScriptChangesClicked()
{
    if (!CachedParsedData.IsValid())
    {
        AppendToConsoleLog(TEXT("\n\nSystem: No script plan is currently pending."));
        return FReply::Handled();
    }

    // 1. Purge cached JSON data so Apply can't run it
    CachedParsedData.Reset();

    // 2. Feedback in UI console
    AppendToConsoleLog(TEXT("\n\nSystem: Pending script changes discarded by user. Ready for next prompt."));

    // 3. Optional: Sync with chat history subsystem
    if (GEditor)
    {
        if (UChatHistoryManager* HistoryMgr = GEditor->GetEditorSubsystem<UChatHistoryManager>())
        {
            HistoryMgr->AddChatMessage(TEXT("System"), TEXT("Pending script changes discarded by user."));
        }
    }

    return FReply::Handled();
}

EAssetConflictResolution SLocalStudioChatPanel::PromptAssetConflictDialog(const FString& ExistingAssetName, FString& OutNewName)
{
    EAssetConflictResolution UserSelection = EAssetConflictResolution::Cancel;
    FString ProposedName = FLocalStudioAssetUtils::GetUniqueAssetName(ExistingAssetName, TEXT("/Game/Materials"));

    TSharedRef<SWindow> ModalWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("Asset Conflict Detected")))
        .ClientSize(FVector2D(460, 160))
        .SizingRule(ESizingRule::FixedSize)
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    ModalWindow->SetContent(
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("Menu.Background"))
        .Padding(16.0f)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 12.0f)
                [
                    SNew(STextBlock)
                        .AutoWrapText(true)
                        .Text(FText::FromString(FString::Printf(
                            TEXT("An asset named '%s' already exists in your project.\nHow would you like to proceed?"),
                            *ExistingAssetName)))
                ]

            + SVerticalBox::Slot().FillHeight(1.0f)

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(0, 0, 4.0f, 0)
                        [
                            SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                                .HAlign(HAlign_Center)
                                .OnClicked_Lambda([&]() {
                                UserSelection = EAssetConflictResolution::Overwrite;
                                ModalWindow->RequestDestroyWindow();
                                return FReply::Handled();
                                    })
                                [
                                    SNew(STextBlock).Text(FText::FromString(TEXT("Overwrite")))
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(2.0f, 0, 2.0f, 0)
                        [
                            SNew(SButton)
                                .HAlign(HAlign_Center)
                                .OnClicked_Lambda([&]() {
                                UserSelection = EAssetConflictResolution::SaveAsNew;
                                OutNewName = ProposedName;
                                ModalWindow->RequestDestroyWindow();
                                return FReply::Handled();
                                    })
                                [
                                    SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Save as %s"), *ProposedName)))
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(4.0f, 0, 0, 0)
                        [
                            SNew(SButton)
                                .HAlign(HAlign_Center)
                                .OnClicked_Lambda([&]() {
                                UserSelection = EAssetConflictResolution::Cancel;
                                ModalWindow->RequestDestroyWindow();
                                return FReply::Handled();
                                    })
                                [
                                    SNew(STextBlock).Text(FText::FromString(TEXT("Cancel")))
                                ]
                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(ModalWindow, FSlateApplication::Get().GetActiveTopLevelWindow());

    return UserSelection;
}