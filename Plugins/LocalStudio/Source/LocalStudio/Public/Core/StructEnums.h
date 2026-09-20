#pragma once

#include "CoreMinimal.h"
#include "StructEnums.generated.h"

class ALandscape;


UENUM(BlueprintType)
enum class ELandscapeBuildMode : uint8
{
    FromDescription,
    FromRealWorldCoords
};

// Enum for dialog user selection
enum class EAssetConflictResolution
{
    Overwrite,
    SaveAsNew,
    Cancel
};

// ==========================================
// LANDSCAPE PROFILE ACTION DATA ONLY
// ==========================================
USTRUCT(BlueprintType)
struct FLandscapeActionData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    FString LandscapeName;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    FString LandscapePackagePath;

    // Grid sizes
    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    int32 SectionSize = 63;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    int32 SectionsPerComponent = 1;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    int32 ComponentsX = 8;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    int32 ComponentsY = 8;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    FString LandscapeRawJson;

    // Real-World Import Fields
    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    bool bIsRealWorld = false;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    double Latitude = 0.0;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    double Longitude = 0.0;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Landscape")
    double DesiredWidthMiles = 2.0;
};

// ==========================================
// MATERIAL PROFILE ACTION DATA ONLY
// ==========================================
USTRUCT(BlueprintType)
struct FMaterialActionData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Material")
    FString MaterialName;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Material")
    FString MaterialPackagePath; // = TEXT("/Game/LocalStudio/Materials/MaterialInstance");

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Material")
    bool bIsMaterialInstance = false;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio|Material")
    FString ParentMaterialPath; // Optional: e.g. "/Game/LocalStudio/Materials/Material/M_MossyBrick"
};

// ==========================================
// SYSTEM SETTINGS & UTILITIES
// ==========================================

/**
USTRUCT(BlueprintType)
struct FOllamaModelSettings
{
    GENERATED_BODY()

    FOllamaModelSettings()
        : OllamaHost(TEXT("127.0.0.1"))
        , OllamaPort(11434)
        , CustomModelStoragePath(TEXT(""))
        , ReasoningModel(TEXT(""))
        , ImageGenerationModel(TEXT(""))
        , CppCodeModel(TEXT(""))
        , BlueprintConversionModel(TEXT(""))
        , LandscapeGenerationModel(TEXT(""))
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
    FString OllamaHost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
    int32 OllamaPort;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
    FString CustomModelStoragePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
    FString ReasoningModel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
    FString ImageGenerationModel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
    FString CppCodeModel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
    FString BlueprintConversionModel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ollama")
    FString LandscapeGenerationModel;
};
*/
// ==========================================
// THE UNIFIED PLAN
// ==========================================
USTRUCT(BlueprintType)
struct FOllamaUnifiedPlan
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio")
    FString Summary;

    // Action Flags
    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio")
    bool bHasLandscapeAction = false;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio")
    bool bHasMaterialAction = false;

    // Isolated Payloads
    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio")
    FLandscapeActionData LandscapeData;

    UPROPERTY(BlueprintReadWrite, Category = "LocalStudio")
    FMaterialActionData MaterialData;
};