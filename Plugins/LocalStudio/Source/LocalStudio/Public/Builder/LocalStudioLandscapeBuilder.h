#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"
#include "Core/StructEnums.h"

#include "Landscape.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

class LOCALSTUDIO_API FLocalStudioLandscapeBuilder : public ILocalStudioBuilder, public TSharedFromThis<FLocalStudioLandscapeBuilder>
{
public:
    // ILocalStudioBuilder Interface
    virtual FName GetBuilderID() const override { return "Landscape"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "LandscapeMode", "Landscape Generation"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "LandscapeHint", "Describe terrain (e.g., 'Volcanic crater 4 miles wide' or real coordinates)..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override
    {
        return TEXT("You are an Unreal Engine Terrain Architect. Generate landscape configuration JSON.");
    }

    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;

    // Execution Helpers
    bool ExecuteLandscapePlan(const FLandscapeActionData& LandscapeData);
    bool ExtractPlan(const FString& PlanJson, FOllamaUnifiedPlan& OutPlan, FString& OutError);

    // Core Generation Methods
    ALandscape* BuildLandscapeFromScript(const FString& AssetName, const FString& PackagePath, const FString& JsonScript);
    //ALandscape* BuildProceduralLandscape(const FString& AssetName, TSharedPtr<FJsonObject> SettingsObj);
    ALandscape* CreateLandscapeActor(UWorld* World, const FString& AssetName, int32 SizeX, int32 SizeY, int32 SectionsPerComponent, int32 SectionSize, const TArray<uint16>& HeightData, double CustomZScale);

    // MapTiler Real-World Terrain Import Methods
    void ImportRealWorldTerrain(double CenterLat, double CenterLon, int32 GridSizeX, int32 GridSizeY, double DesiredWidthMiles, const FString& InAssetName);
    void FetchTileBatch();
    void OnTileResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FIntPoint TileCoord);
    void FinalizeLandscapeImport();

    void StartBuildProcess(ELandscapeBuildMode Mode);
    void ProcessRealWorldHeightData(const TArray<float>& ElevationDataMeters, int32 Width, int32 Height, double MinElevMeters, double MaxElevMeters);

    // Helper math utilities for MapTiler Web Mercator conversions
    static FIntPoint LatLonToTile(double Lat, double Lon, int32 Zoom);
    static float DecodeTerrainRGB(uint8 R, uint8 G, uint8 B);

    FCriticalSection TileMapLock;

private:
    // API Configuration
    FString MapTilerApiKey = TEXT("YOUR_MAPTILER_API_KEY_HERE"); // Replace or wire to plugin settings

    // Real-World Terrain Import State
    double CachedDesiredWidthMiles = 2.0;
    FString TargetAssetName;
    int32 TargetSizeX = 0;
    int32 TargetSizeY = 0;
    int32 LowResSizeX = 64;
    int32 LowResSizeY = 64;

    double MapCenterLat = 0.0;
    double MapCenterLon = 0.0;
    int32 MapZoomLevel = 12;

    TArray<FIntPoint> RequiredTiles;
    TMap<FIntPoint, TArray<uint8>> DownloadedTilePNGs;
    int32 PendingTileDownloads = 0;

    TArray<float> AccumulatedHeights;
};