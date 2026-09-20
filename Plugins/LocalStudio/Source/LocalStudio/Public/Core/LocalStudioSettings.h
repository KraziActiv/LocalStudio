#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "LocalStudioSettings.generated.h"

// ==========================================
// ENUMS & STRUCTS FOR SETTINGS
// ==========================================

UENUM(BlueprintType)
enum class EOllamaComputeDevice : uint8
{
    GPU,
    CPU
};

USTRUCT(BlueprintType)
struct FOllamaTaskProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama", meta = (ToolTip = "The specific model used for this task."))
    FString ModelName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama", meta = (MultiLine = true, ToolTip = "System prompt steering instruction for this specialized role."))
    FString JobDescription;
};

DECLARE_MULTICAST_DELEGATE(FOnLocalStudioSettingsChanged);

// ==========================================
// MASTER SETTINGS CLASS
// ==========================================

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Local Studio Settings"))
class LOCALSTUDIO_API ULocalStudioSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    ULocalStudioSettings();

    // --- MAPTILER & REAL-WORLD TERRAIN SETTINGS ---
    UPROPERTY(EditAnywhere, config, Category = "MapTiler Terrain Importer", meta = (DisplayName = "MapTiler API Key", ToolTip = "Get your free API key from maptiler.com."))
    FString MapTilerApiKey;

    UPROPERTY(EditAnywhere, config, Category = "MapTiler Terrain Importer", meta = (ClampMin = "0.5", ClampMax = "100.0"))
    float DefaultMapWidthKm;

    UPROPERTY(EditAnywhere, config, Category = "MapTiler Terrain Importer", meta = (ClampMin = "0.5", ClampMax = "100.0"))
    float DefaultMapHeightKm;

    UPROPERTY(EditAnywhere, config, Category = "MapTiler Terrain Importer")
    int32 DefaultLandscapeResolution;

    // --- CONNECTION SETTINGS ---
    UPROPERTY(EditAnywhere, config, Category = "Ollama Connection", meta = (DisplayName = "Host Address"))
    FString OllamaHost;

    UPROPERTY(EditAnywhere, config, Category = "Ollama Connection", meta = (DisplayName = "Port Number"))
    int32 OllamaPort;

    UPROPERTY(EditAnywhere, config, Category = "Ollama Connection", meta = (DisplayName = "Compute Device Target"))
    EOllamaComputeDevice ComputeDevice;

    // --- SYSTEM TIMEOUT OVERRIDES ---
    UPROPERTY(EditAnywhere, config, Category = "Network Timeouts", meta = (DisplayName = "HTTP Timeout Duration (Seconds)"))
    float HttpTimeoutDuration;

    UPROPERTY(EditAnywhere, config, Category = "Network Timeouts")
    bool bEnableHttp;

    // Derived timeout values
    float HttpActivityTimeout;
    float HttpConnectionTimeout;

    // --- AGENT PROFILES ---
    UPROPERTY(EditAnywhere, config, Category = "Task Profiles")
    TMap<FString, FOllamaTaskProfile> TaskProfiles;

    // --- DEFAULTS & INFERENCE PARAMETERS ---
    UPROPERTY(EditAnywhere, config, Category = "Inference Default Settings")
    int32 ContextWindow;

    UPROPERTY(EditAnywhere, config, Category = "Inference Default Settings")
    int32 MaxTokens;

    UPROPERTY(EditAnywhere, config, Category = "Inference Default Settings", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Temperature;

    UPROPERTY(EditAnywhere, config, Category = "Paths", meta = (RelativeToGameDir = false))
    FDirectoryPath EnginePath;

    FOnLocalStudioSettingsChanged OnSettingsChanged;

    FString GetAssembledSystemPrompt(const FString& ProfileKey) const;
    void ApplyGlobalHttpOverrides() const;

    /** Optional custom path to Unreal Engine source directory. If left empty, FPaths::EngineDir() will be used automatically. */
    UPROPERTY(EditAnywhere, Config, Category = "Engine Integration", meta = (FilePathFilter = "Directory"))
    FString EnginePathOverride;

public:
    virtual FName GetContainerName() const override { return TEXT("Project"); }
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
    virtual FName GetSectionName() const override { return TEXT("Local Studio"); }

#if WITH_EDITOR
    virtual FText GetSectionText() const override { return NSLOCTEXT("LocalStudio", "LocalStudioSettingsSection", "Local Studio"); }
    virtual FText GetSectionDescription() const override { return NSLOCTEXT("LocalStudio", "LocalStudioSettingsDescription", "Configure MapTiler API and Ollama settings."); }

    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    /** Static instance handler for inline Details View Customization */
    static TSharedRef<class IDetailCustomization> MakeInstance();
#endif
};