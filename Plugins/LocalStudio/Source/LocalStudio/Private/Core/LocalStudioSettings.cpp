#include "Core/LocalStudioSettings.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"

#if WITH_EDITOR
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "UnrealEdMisc.h"

// ==========================================
// INLINE SETTINGS DETAILS CUSTOMIZATION
// ==========================================

class FLocalStudioSettingsCustomization : public IDetailCustomization
{
public:
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
    {
        IDetailCategoryBuilder& UtilitiesCategory = DetailBuilder.EditCategory(
            "LocalStudio Actions",
            FText::GetEmpty(),
            ECategoryPriority::Transform
        );

        UtilitiesCategory.AddCustomRow(FText::FromString(TEXT("Restart Operations")))
            .NameContent()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Apply Settings Changes")))
                    .Font(IDetailLayoutBuilder::GetDetailFont())
            ]
            .ValueContent()
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Save & Restart Editor")))
                    .ContentPadding(FMargin(10.0f, 4.0f))
                    .OnClicked_Lambda([]() -> FReply
                        {
                            if (ULocalStudioSettings* ConfigSettings = GetMutableDefault<ULocalStudioSettings>())
                            {
                                ConfigSettings->ApplyGlobalHttpOverrides();
                                ConfigSettings->TryUpdateDefaultConfigFile(ConfigSettings->GetDefaultConfigFilename());
                            }

                            FUnrealEdMisc::Get().RestartEditor(true);
                            return FReply::Handled();
                        })
            ];
    }
};

TSharedRef<IDetailCustomization> ULocalStudioSettings::MakeInstance()
{
    return MakeShared<FLocalStudioSettingsCustomization>();
}
#endif

// ==========================================
// ULOCALSTUDIOSETTINGS IMPLEMENTATION
// ==========================================

ULocalStudioSettings::ULocalStudioSettings()
{
    CategoryName = TEXT("Plugins");
    SectionName = TEXT("Local Studio");

    MapTilerApiKey = TEXT("");
    DefaultMapWidthKm = 8.0f;
    DefaultMapHeightKm = 8.0f;
    DefaultLandscapeResolution = 1009;

    OllamaHost = TEXT("127.0.0.1");
    OllamaPort = 11434;

    HttpTimeoutDuration = 3600.0f;
    HttpActivityTimeout = HttpTimeoutDuration;
    HttpConnectionTimeout = HttpTimeoutDuration;
    bEnableHttp = true;

    ComputeDevice = EOllamaComputeDevice::GPU;
    ContextWindow = 4096;
    MaxTokens = 2048;
    Temperature = 0.7f;

    // --- POPULATE ALL BUILDER TASK PROFILES ---
    TArray<FString> DefaultProfiles = {
        TEXT("GeneralChat"), TEXT("Material"), TEXT("Landscape"), TEXT("Foliage"),
        TEXT("LevelDesign"), TEXT("StaticMesh"), TEXT("Niagara"), TEXT("Blueprint"),
        TEXT("Animation"), TEXT("GameplayAbility"), TEXT("Audio"), TEXT("UI"),
        TEXT("ProjectConfig"), TEXT("MetaHuman"), TEXT("Code"), TEXT("PCG"), TEXT("Custom")
    };

    for (const FString& ProfileKey : DefaultProfiles)
    {
        FOllamaTaskProfile& Profile = TaskProfiles.FindOrAdd(ProfileKey);
        if (Profile.ModelName.IsEmpty())
        {
            Profile.ModelName = TEXT("qwen3-coder:30b");
        }
        if (Profile.JobDescription.IsEmpty())
        {
            Profile.JobDescription = FString::Printf(TEXT("You are LocalStudio's %s Architect."), *ProfileKey);
        }
    }
}

FString ULocalStudioSettings::GetAssembledSystemPrompt(const FString& ProfileKey) const
{
    const FOllamaTaskProfile* Profile = TaskProfiles.Find(ProfileKey);
    return Profile ? Profile->JobDescription : TEXT("");
}

void ULocalStudioSettings::ApplyGlobalHttpOverrides() const
{
    const FString TargetIniPath = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");
    const FString TimeoutVal = FString::SanitizeFloat(HttpTimeoutDuration);

    // 1. Update GConfig in-memory cache using active non-deprecated keys
    GConfig->SetString(TEXT("HTTP"), TEXT("HttpActivityTimeout"), *TimeoutVal, GEngineIni);
    GConfig->SetString(TEXT("HTTP"), TEXT("HttpConnectionTimeout"), *TimeoutVal, GEngineIni);

    // 2. Load disk file content
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *TargetIniPath))
    {
        FileContent = TEXT("");
    }

    const FString NewHttpBlock = FString::Printf(
        TEXT("[HTTP]\r\nHttpActivityTimeout=%s\r\nHttpConnectionTimeout=%s\r\n"),
        *TimeoutVal, *TimeoutVal
    );

    // Search for explicit section header line
    int32 HttpSectionIndex = FileContent.Find(TEXT("[HTTP]\r\n"));
    if (HttpSectionIndex == INDEX_NONE)
    {
        HttpSectionIndex = FileContent.Find(TEXT("[HTTP]\n"));
    }

    if (HttpSectionIndex != INDEX_NONE)
    {
        // Find end of section (start of next '[' section)
        int32 NextSectionIndex = FileContent.Find(TEXT("["), ESearchCase::IgnoreCase, ESearchDir::FromStart, HttpSectionIndex + 6);

        if (NextSectionIndex != INDEX_NONE)
        {
            FileContent.RemoveAt(HttpSectionIndex, NextSectionIndex - HttpSectionIndex);
            FileContent.InsertAt(HttpSectionIndex, NewHttpBlock + TEXT("\r\n"));
        }
        else
        {
            FileContent.RemoveAt(HttpSectionIndex, FileContent.Len() - HttpSectionIndex);
            FileContent.Append(NewHttpBlock);
        }
    }
    else
    {
        if (!FileContent.IsEmpty() && !FileContent.EndsWith(TEXT("\n")))
        {
            FileContent.Append(TEXT("\r\n\r\n"));
        }
        FileContent.Append(NewHttpBlock);
    }

    // 3. Write back to file directly
    if (FFileHelper::SaveStringToFile(FileContent, *TargetIniPath, FFileHelper::EEncodingOptions::ForceAnsi))
    {
        UE_LOG(LogTemp, Log, TEXT("[LocalStudio] Injected unified HTTP timeouts (%s s) into DefaultEngine.ini."), *TimeoutVal);
    }
}

#if WITH_EDITOR
void ULocalStudioSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    HttpActivityTimeout = HttpTimeoutDuration;
    HttpConnectionTimeout = HttpTimeoutDuration;

    SaveConfig();

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
    const FName TargetPropName = GET_MEMBER_NAME_CHECKED(ULocalStudioSettings, HttpTimeoutDuration);

    // Check both property and member property name to handle all editor UI change events
    if (PropertyName == TargetPropName || MemberPropertyName == TargetPropName)
    {
        ApplyGlobalHttpOverrides();
    }

    OnSettingsChanged.Broadcast();
}
#endif