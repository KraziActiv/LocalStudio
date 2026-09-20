#include "Builder/LocalStudioLandscapeBuilder.h"

#include "Core/LocalStudioSettings.h"
#include "Core/StructEnums.h"

// JSON Parsing Utilities
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

// Landscape Utilities
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeDataAccess.h"

// Editor Utilities
#include "Editor.h"
#include "ScopedTransaction.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "Selection.h"

// Image Processing & Decompression
#include "ImageUtils.h"
#include "ImageCore.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

namespace LocalStudioNoise
{
    static const int32* GetPermutationTable()
    {
        static int32 Permutation[512];
        static bool bInitialized = false;

        if (!bInitialized)
        {
            const int32 BasePermutation[256] = {
                151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
                190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,
                20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,
                230,220,105,92,41,55,46,245,40,244,102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,
                18,169,200,196,135,130,116,188,26,2,242,114,249,24,198,224,97,228,251,34,242,193,238,210,144,12,
                191,179,162,241, 81,51,145,235,249,14,239,107,49,192,214, 31,181,199,106,157,184, 84,204,176,115,
                121,50,45,127, 4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
            };

            for (int32 i = 0; i < 256; ++i)
            {
                Permutation[i] = BasePermutation[i];
                Permutation[i + 256] = BasePermutation[i];
            }
            bInitialized = true;
        }

        return Permutation;
    }

    FORCEINLINE double Fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    FORCEINLINE double Lerp(double t, double a, double b) { return a + t * (b - a); }
    FORCEINLINE double Grad(int32 Hash, double x, double y)
    {
        const int32 h = Hash & 7;
        const double u = h < 4 ? x : y;
        const double v = h < 4 ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -2.0 * v : 2.0 * v);
    }

    double PerlinNoise2D(double x, double y)
    {
        const int32* Perm = GetPermutationTable();
        const int32 X = FMath::FloorToInt(x) & 255;
        const int32 Y = FMath::FloorToInt(y) & 255;

        x -= FMath::FloorToDouble(x);
        y -= FMath::FloorToDouble(y);

        const double u = Fade(x);
        const double v = Fade(y);

        const int32 A = Perm[X] + Y;
        const int32 B = Perm[X + 1] + Y;

        return Lerp(v, Lerp(u, Grad(Perm[A], x, y), Grad(Perm[B], x - 1, y)),
            Lerp(u, Grad(Perm[A + 1], x, y - 1), Grad(Perm[B + 1], x - 1, y - 1))) * 0.5;
    }

    double FractionalBrownianMotion(double x, double y, int32 Octaves, double Lacunarity, double Gain)
    {
        double Total = 0.0;
        double Amplitude = 1.0;
        double Frequency = 1.0;
        double MaxValue = 0.0;

        for (int32 i = 0; i < Octaves; ++i)
        {
            Total += PerlinNoise2D(x * Frequency, y * Frequency) * Amplitude;
            MaxValue += Amplitude;
            Amplitude *= Gain;
            Frequency *= Lacunarity;
        }

        return Total / MaxValue;
    }
}

FIntPoint FLocalStudioLandscapeBuilder::LatLonToTile(double Lat, double Lon, int32 Zoom)
{
    // Clamp latitude to valid Web Mercator boundaries to prevent NaN from log/tan math
    double ClampedLat = FMath::Clamp(Lat, -85.05112877980659, 85.05112877980659);
    double LatRad = FMath::DegreesToRadians(ClampedLat);
    double N = FMath::Pow(2.0, Zoom);
    int32 TileX = FMath::FloorToInt((Lon + 180.0) / 360.0 * N);
    int32 TileY = FMath::FloorToInt((1.0 - FMath::Loge(FMath::Tan(LatRad) + (1.0 / FMath::Cos(LatRad))) / UE_PI) / 2.0 * N);
    return FIntPoint(TileX, TileY);
}

float FLocalStudioLandscapeBuilder::DecodeTerrainRGB(uint8 R, uint8 G, uint8 B)
{
    // Standard MapTiler Terrain-RGB formula
    return -10000.0f + ((R * 65536.0f + G * 256.0f + B) * 0.1f);
}

ALandscape* FLocalStudioLandscapeBuilder::BuildLandscapeFromScript(const FString& AssetName, const FString& PackagePath, const FString& JsonScript)
{
    UWorld* World = nullptr;
    if (GEditor) { World = GEditor->GetEditorWorldContext().World(); }
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("LocalStudio: Failed to find a valid Editor World."));
        return nullptr;
    }

    // --- CHECK FOR EXISTING TARGET LANDSCAPE TO PRESERVE ITS SIZE & LOCATION ---
    ALandscape* ExistingLandscape = nullptr;
    if (GEditor)
    {
        TArray<UObject*> SelectedActors;
        GEditor->GetSelectedActors()->GetSelectedObjects(ALandscape::StaticClass(), SelectedActors);
        if (SelectedActors.Num() > 0) { ExistingLandscape = Cast<ALandscape>(SelectedActors[0]); }
    }

    if (!ExistingLandscape)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            if (It->GetName().Equals(AssetName) || It->GetActorLabel().Equals(AssetName))
            {
                ExistingLandscape = *It;
                break;
            }
        }
    }

    // --- 1. DEFAULT PARAMETERS ---
    double MountainWeight = 1.0;
    double ElevationScale = 12000.0;
    double NoiseScale = 0.0008;
    int32 Octaves = 4;
    double Lacunarity = 2.1;
    double Gain = 0.45;
    int32 Seed = 1337;

    double ValleyFlattening = 0.6;
    double PlateauClamping = 0.95;
    double SlopePower = 1.3;

    double DesiredWidthMiles = -1.0;

    bool bHasOcean = false;
    bool bHasRiver = false;
    bool bHasValleyFloor = false;

    // --- 2. JSON PARSING PASS ---
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonScript);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        if (JsonObject->HasTypedField<EJson::Object>(TEXT("landscape_action")))
        {
            JsonObject = JsonObject->GetObjectField(TEXT("landscape_action"));
        }

        double TempVal = 0.0;
        if (JsonObject->TryGetNumberField(TEXT("desired_width_miles"), TempVal)) { DesiredWidthMiles = TempVal; }
        else if (JsonObject->TryGetNumberField(TEXT("size_miles"), TempVal)) { DesiredWidthMiles = TempVal; }
        else if (JsonObject->TryGetNumberField(TEXT("square_miles"), TempVal) || JsonObject->TryGetNumberField(TEXT("size"), TempVal))
        {
            double SqMiles = JsonObject->HasField(TEXT("square_miles")) ? JsonObject->GetNumberField(TEXT("square_miles")) : JsonObject->GetNumberField(TEXT("size"));
            DesiredWidthMiles = FMath::Sqrt(FMath::Max(0.1, SqMiles));
        }

        if (JsonObject->HasField(TEXT("mountain_weight"))) MountainWeight = FMath::Clamp(JsonObject->GetNumberField(TEXT("mountain_weight")), 0.0, 1.0);
        if (JsonObject->TryGetNumberField(TEXT("elevation_scale"), TempVal)) ElevationScale = TempVal;
        if (JsonObject->TryGetNumberField(TEXT("noise_scale"), TempVal)) NoiseScale = TempVal;
        if (JsonObject->TryGetNumberField(TEXT("octaves"), TempVal)) Octaves = FMath::Clamp((int32)TempVal, 1, 10);
        if (JsonObject->TryGetNumberField(TEXT("lacunarity"), TempVal)) Lacunarity = TempVal;
        if (JsonObject->TryGetNumberField(TEXT("gain"), TempVal)) Gain = TempVal;

        if (JsonObject->TryGetNumberField(TEXT("seed"), TempVal)) { Seed = (int32)TempVal; }
        else { Seed = FMath::Rand(); }

        if (JsonObject->TryGetNumberField(TEXT("valley_flattening"), TempVal)) ValleyFlattening = FMath::Clamp(TempVal, 0.0, 1.0);
        if (JsonObject->TryGetNumberField(TEXT("plateau_clamping"), TempVal)) PlateauClamping = FMath::Clamp(TempVal, 0.0, 1.0);
        if (JsonObject->TryGetNumberField(TEXT("slope_power"), TempVal)) SlopePower = FMath::Max(0.1, TempVal);

        if (JsonObject->HasField(TEXT("has_ocean"))) bHasOcean = JsonObject->GetBoolField(TEXT("has_ocean"));
        if (JsonObject->HasField(TEXT("has_river"))) bHasRiver = JsonObject->GetBoolField(TEXT("has_river"));
        if (JsonObject->HasField(TEXT("has_valley"))) bHasValleyFloor = JsonObject->GetBoolField(TEXT("has_valley"));
    }

    if (!bHasOcean && JsonScript.Contains(TEXT("ocean"), ESearchCase::IgnoreCase)) bHasOcean = true;
    if (!bHasValleyFloor && (JsonScript.Contains(TEXT("valley"), ESearchCase::IgnoreCase) || JsonScript.Contains(TEXT("base"), ESearchCase::IgnoreCase))) bHasValleyFloor = true;

    if (DesiredWidthMiles <= 0.0)
    {
        if (ExistingLandscape)
        {
            FVector ActorScale = ExistingLandscape->GetActorScale3D();
            FIntRect Bounds = ExistingLandscape->GetBoundingRect();
            if (Bounds.Width() > 0)
            {
                double TotalWidthCM = static_cast<double>(Bounds.Width()) * 100.0 * ActorScale.X;
                DesiredWidthMiles = TotalWidthCM / 160934.4;
            }
            else
            {
                DesiredWidthMiles = 2.0;
            }
        }
        else
        {
            DesiredWidthMiles = 2.0;
        }
    }

    // --- 3. AUTO-CALCULATE UNREAL TOPOLOGY ---
    int32 SectionSize = 127;
    int32 SectionsPerComponent = 2;
    int32 ComponentCountX = 16;
    int32 ComponentCountY = 16;

    if (DesiredWidthMiles >= 15.0)
    {
        ComponentCountX = 32;
        ComponentCountY = 32;
        SectionSize = 127;
    }
    else if (DesiredWidthMiles >= 10.0)
    {
        ComponentCountX = 16;
        ComponentCountY = 16;
        SectionSize = 127;
    }
    else if (DesiredWidthMiles >= 5.0)
    {
        ComponentCountX = 8;
        ComponentCountY = 8;
        SectionSize = 127;
    }

    int32 QuadsPerComponent = SectionSize * SectionsPerComponent;
    int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
    int32 SizeY = ComponentCountY * QuadsPerComponent + 1;

    // --- 4. HORIZONTAL METRIC SCALING ---
    double RealWorldWidthCentimeters = DesiredWidthMiles * 160934.4;
    double TotalQuadsX = static_cast<double>(SizeX - 1);
    double CalculatedXYScale = RealWorldWidthCentimeters / (TotalQuadsX * 100.0);
    CalculatedXYScale = FMath::Max(0.001, CalculatedXYScale);

    double NoiseStretchingFactor = (DesiredWidthMiles > 2.0) ? (DesiredWidthMiles / 2.0) : 1.0;
    double AdjustedNoiseScale = NoiseScale / NoiseStretchingFactor;

    double OffsetX = (double)Seed * 1234.56;
    double OffsetY = (double)Seed * 789.10;

    TArray<uint16> HeightData;
    HeightData.SetNum(SizeX * SizeY);

    TArray<double> ProcessedNoiseArray;
    ProcessedNoiseArray.SetNum(SizeX * SizeY);

    const double SeaLevelNormalized = 0.25;

    // --- 5. NOISE & FEATURE MASK GENERATION ---
    for (int32 y = 0; y < SizeY; ++y)
    {
        for (int32 x = 0; x < SizeX; ++x)
        {
            double SampleX = (static_cast<double>(x) * AdjustedNoiseScale) + OffsetX;
            double SampleY = (static_cast<double>(y) * AdjustedNoiseScale) + OffsetY;

            double U = static_cast<double>(x) / static_cast<double>(SizeX - 1);
            double V = static_cast<double>(y) / static_cast<double>(SizeY - 1);

            double BaseNoise = LocalStudioNoise::FractionalBrownianMotion(SampleX * 0.5, SampleY * 0.5, 4, 2.0, 0.5);
            double RollingHills = (BaseNoise + 1.0) * 0.5;

            double MountainSampleX = SampleX * 3.5;
            double MountainSampleY = SampleY * 3.5;

            double N = LocalStudioNoise::PerlinNoise2D(MountainSampleX, MountainSampleY);
            double Signal = 1.0 - FMath::Abs(N);
            Signal = FMath::Pow(Signal, SlopePower);
            double MountainPeaks = Signal;
            double Weight = Signal;

            double Freq = Lacunarity;
            double Amp = Gain;

            for (int32 i = 1; i < Octaves; ++i)
            {
                double NextN = LocalStudioNoise::PerlinNoise2D(MountainSampleX * Freq, MountainSampleY * Freq);
                double NextSignal = 1.0 - FMath::Abs(NextN);
                NextSignal = FMath::Pow(NextSignal, SlopePower);
                NextSignal *= Weight;

                MountainPeaks += NextSignal * Amp;
                Weight = FMath::Clamp(NextSignal, 0.0, 1.0);

                Freq *= Lacunarity;
                Amp *= Gain;
            }

            double SpawnMaskNoise = LocalStudioNoise::PerlinNoise2D(SampleX * 0.8, SampleY * 0.8);
            double MountainSpawnMask = FMath::Clamp((SpawnMaskNoise + 0.2) * 2.0, 0.0, 1.0);

            double BaseHillsStrength = 0.05 + (0.95 * MountainWeight);
            double RawTerrain = (RollingHills * BaseHillsStrength) + (MountainPeaks * 0.85 * MountainSpawnMask * MountainWeight);

            if (bHasValleyFloor)
            {
                double DistFromCenter = FMath::Sqrt(FMath::Square(U - 0.5) + FMath::Square(V - 0.5)) * 2.0;
                double ValleyMask = FMath::SmoothStep(0.15, 0.65, DistFromCenter);
                RawTerrain = FMath::Lerp(RawTerrain * 0.15 + 0.05, RawTerrain, ValleyMask);
            }

            if (bHasOcean)
            {
                double CoastNoise = LocalStudioNoise::PerlinNoise2D(SampleY * 2.0, Seed * 0.1) * 0.08;
                double CoastlinePos = 0.20 + CoastNoise;

                if (U < CoastlinePos)
                {
                    double OceanDepthFactor = (CoastlinePos - U) / CoastlinePos;
                    double OceanFloor = SeaLevelNormalized - (OceanDepthFactor * 0.20);
                    RawTerrain = FMath::Lerp(SeaLevelNormalized, OceanFloor, FMath::SmoothStep(0.0, 1.0, OceanDepthFactor));
                }
                else if (U < CoastlinePos + 0.05)
                {
                    double BeachFactor = (U - CoastlinePos) / 0.05;
                    RawTerrain = FMath::Lerp(SeaLevelNormalized, RawTerrain, FMath::SmoothStep(0.0, 1.0, BeachFactor));
                }
            }

            if (RawTerrain > PlateauClamping)
            {
                double Excess = RawTerrain - PlateauClamping;
                RawTerrain = PlateauClamping + (Excess * 0.15);
            }

            double FinalHeightNormalized = bHasOcean ? RawTerrain : (SeaLevelNormalized + (RawTerrain * (1.0 - SeaLevelNormalized)));
            ProcessedNoiseArray[y * SizeX + x] = FMath::Clamp(FinalHeightNormalized, 0.0, 1.0);
        }
    }

    // --- 6. EROSION PASSES ---
    const int32 ErosionPasses = 3;
    const double TalusThreshold = 0.08;
    const double ErosionRate = 0.20;

    for (int32 Pass = 0; Pass < ErosionPasses; ++Pass)
    {
        for (int32 y = 1; y < SizeY - 1; ++y)
        {
            for (int32 x = 1; x < SizeX - 1; ++x)
            {
                int32 CurrentIdx = y * SizeX + x;
                double CurrentHeight = ProcessedNoiseArray[CurrentIdx];

                int32 Neighbors[4] = {
                    (y - 1) * SizeX + x,
                    (y + 1) * SizeX + x,
                    y * SizeX + (x - 1),
                    y * SizeX + (x + 1)
                };

                for (int32 NeighborIdx : Neighbors)
                {
                    double NeighborHeight = ProcessedNoiseArray[NeighborIdx];
                    double HeightDiff = CurrentHeight - NeighborHeight;

                    if (HeightDiff > TalusThreshold)
                    {
                        double ShiftAmount = (HeightDiff - TalusThreshold) * ErosionRate;
                        CurrentHeight -= ShiftAmount;
                        ProcessedNoiseArray[NeighborIdx] += ShiftAmount;
                    }
                }
                ProcessedNoiseArray[CurrentIdx] = CurrentHeight;
            }
        }
    }

    // --- 7. HEIGHTMAP CONVERSION TO UINT16 ---
    for (int32 y = 0; y < SizeY; ++y)
    {
        for (int32 x = 0; x < SizeX; ++x)
        {
            int32 Index = y * SizeX + x;
            double FinalNoise = ProcessedNoiseArray[Index];

            double MidPoint = 32768.0;
            double HeightRange = 25000.0;

            double ScaledHeight = MidPoint + ((FinalNoise - 0.25) * HeightRange);
            HeightData[Index] = static_cast<uint16>(FMath::Clamp(ScaledHeight, 0.0, 65535.0));
        }
    }

    // --- 8. VERTICAL METRICS & SPAWNING ---
    double DesiredMaxHeightCM = ElevationScale;
    double CalculatedZScale = (DesiredMaxHeightCM / 51200.0) * 100.0;
    CalculatedZScale = FMath::Clamp(CalculatedZScale, 10.0, 200.0);

    ALandscape* SpawnedActor = CreateLandscapeActor(World, AssetName, SizeX, SizeY, SectionsPerComponent, SectionSize, HeightData, CalculatedZScale);

    if (SpawnedActor)
    {
        FVector FinalScale(CalculatedXYScale, CalculatedXYScale, CalculatedZScale);

        if (USceneComponent* RootComp = SpawnedActor->GetRootComponent())
        {
            RootComp->SetWorldScale3D(FinalScale);
        }

        SpawnedActor->PostEditChange();
    }

    return SpawnedActor;
}

bool FLocalStudioLandscapeBuilder::ExtractPlan(const FString& PlanJson, FOllamaUnifiedPlan& OutPlan, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PlanJson);
    TSharedPtr<FJsonObject> RootObject;

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutError = TEXT("Failed to parse response as valid JSON.");
        return false;
    }

    RootObject->TryGetStringField(TEXT("summary"), OutPlan.Summary);

    const TSharedPtr<FJsonObject>* LandscapeActionObj = nullptr;
    if (RootObject->TryGetObjectField(TEXT("landscape_action"), LandscapeActionObj) && LandscapeActionObj)
    {
        OutPlan.bHasLandscapeAction = true;

        (*LandscapeActionObj)->TryGetStringField(TEXT("name"), OutPlan.LandscapeData.LandscapeName);
        (*LandscapeActionObj)->TryGetStringField(TEXT("package_path"), OutPlan.LandscapeData.LandscapePackagePath);

        double TempVal = 0.0;
        if ((*LandscapeActionObj)->TryGetNumberField(TEXT("section_size"), TempVal)) OutPlan.LandscapeData.SectionSize = (int32)TempVal;
        if ((*LandscapeActionObj)->TryGetNumberField(TEXT("sections_per_component"), TempVal)) OutPlan.LandscapeData.SectionsPerComponent = (int32)TempVal;
        if ((*LandscapeActionObj)->TryGetNumberField(TEXT("components_x"), TempVal)) OutPlan.LandscapeData.ComponentsX = (int32)TempVal;
        if ((*LandscapeActionObj)->TryGetNumberField(TEXT("components_y"), TempVal)) OutPlan.LandscapeData.ComponentsY = (int32)TempVal;

        FString ImportType;
        if ((*LandscapeActionObj)->TryGetStringField(TEXT("import_type"), ImportType) && ImportType.Equals(TEXT("real_world")))
        {
            OutPlan.LandscapeData.bIsRealWorld = true;
            (*LandscapeActionObj)->TryGetNumberField(TEXT("latitude"), OutPlan.LandscapeData.Latitude);
            (*LandscapeActionObj)->TryGetNumberField(TEXT("longitude"), OutPlan.LandscapeData.Longitude);

            double ReadSize = 0.0;
            if ((*LandscapeActionObj)->TryGetNumberField(TEXT("desired_width_miles"), ReadSize) ||
                (*LandscapeActionObj)->TryGetNumberField(TEXT("size_miles"), ReadSize))
            {
                OutPlan.LandscapeData.DesiredWidthMiles = ReadSize;
            }
            else
            {
                OutPlan.LandscapeData.DesiredWidthMiles = 2.0;
            }
        }

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutPlan.LandscapeData.LandscapeRawJson);
        FJsonSerializer::Serialize((*LandscapeActionObj).ToSharedRef(), Writer);
    }

    const TSharedPtr<FJsonObject>* MaterialActionObj = nullptr;
    if (RootObject->TryGetObjectField(TEXT("material_action"), MaterialActionObj) && MaterialActionObj)
    {
        OutPlan.bHasMaterialAction = true;
        (*MaterialActionObj)->TryGetStringField(TEXT("name"), OutPlan.MaterialData.MaterialName);
        (*MaterialActionObj)->TryGetStringField(TEXT("package_path"), OutPlan.MaterialData.MaterialPackagePath);
    }

    if (!OutPlan.bHasLandscapeAction && !OutPlan.bHasMaterialAction)
    {
        OutError = OutPlan.Summary.IsEmpty() ? TEXT("The plan has no executable actions.") : OutPlan.Summary;
        return false;
    }

    return true;
}

void FLocalStudioLandscapeBuilder::ImportRealWorldTerrain(double CenterLat, double CenterLon, int32 GridSizeX, int32 GridSizeY, double DesiredWidthMiles, const FString& InAssetName)
{

    // --- FORCE FETCH MAPTILER API KEY FROM SETTINGS ---
    if (const ULocalStudioSettings* Settings = GetDefault<ULocalStudioSettings>())
    {
        MapTilerApiKey = Settings->MapTilerApiKey.TrimStartAndEnd();
    }

    if (MapTilerApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("LocalStudio: MapTiler API Key is missing! Please enter a valid key in Project Settings -> LocalStudio."));
        return;
    }

    // Sanity check log to verify the key being used (prints first 4 chars for safety)
    FString KeyPrefix = MapTilerApiKey.Left(4);
    UE_LOG(LogTemp, Log, TEXT("LocalStudio: Using MapTiler Key starting with: %s****"), *KeyPrefix);

    CachedDesiredWidthMiles = DesiredWidthMiles;
    TargetAssetName = InAssetName.IsEmpty() ? TEXT("RealWorldLandscape") : InAssetName;
    TargetSizeX = GridSizeX;
    MapCenterLat = CenterLat;
    MapCenterLon = CenterLon;

    // Pick appropriate high zoom level to ensure high density height data
    if (DesiredWidthMiles >= 15.0) MapZoomLevel = 12;
    else if (DesiredWidthMiles >= 8.0) MapZoomLevel = 13;
    else MapZoomLevel = 15;

    // Calculate bounding box in Lat/Lon
    double TotalWidthMeters = DesiredWidthMiles * 1609.34;
    double HalfWidthMeters = TotalWidthMeters / 2.0;

    const double MetersPerDegreeLat = 111000.0;
    const double MetersPerDegreeLon = 111000.0 * FMath::Cos(FMath::DegreesToRadians(CenterLat));

    double MinLat = CenterLat - (HalfWidthMeters / MetersPerDegreeLat);
    double MaxLat = CenterLat + (HalfWidthMeters / MetersPerDegreeLat);
    double MinLon = CenterLon - (HalfWidthMeters / MetersPerDegreeLon);
    double MaxLon = CenterLon + (HalfWidthMeters / MetersPerDegreeLon);

    FIntPoint TopLeftTile = LatLonToTile(MaxLat, MinLon, MapZoomLevel);
    FIntPoint BottomRightTile = LatLonToTile(MinLat, MaxLon, MapZoomLevel);

    RequiredTiles.Empty();
    DownloadedTilePNGs.Empty();

    for (int32 TileY = TopLeftTile.Y; TileY <= BottomRightTile.Y; ++TileY)
    {
        for (int32 TileX = TopLeftTile.X; TileX <= BottomRightTile.X; ++TileX)
        {
            RequiredTiles.Add(FIntPoint(TileX, TileY));
        }
    }

    PendingTileDownloads = RequiredTiles.Num();

    UE_LOG(LogTemp, Log, TEXT("LocalStudio: Requesting %d MapTiler terrain tiles (Zoom Level %d)..."), PendingTileDownloads, MapZoomLevel);

    FetchTileBatch();
}

void FLocalStudioLandscapeBuilder::FetchTileBatch()
{
    FHttpModule* Http = &FHttpModule::Get();

    if (MapTilerApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("LocalStudio: MapTiler API key is empty!"));
        return;
    }

    for (const FIntPoint& TileCoord : RequiredTiles)
    {
        // Request PNG from terrain-rgb endpoint
        FString RequestUrl = FString::Printf(
            TEXT("https://api.maptiler.com/tiles/terrain-rgb/%d/%d/%d.png?key=%s"),
            MapZoomLevel, TileCoord.X, TileCoord.Y, *MapTilerApiKey
        );

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
        Request->SetURL(RequestUrl);
        Request->SetVerb(TEXT("GET"));

        Request->SetHeader(TEXT("User-Agent"), TEXT("UnrealEngine-LocalStudio/1.0"));
        // Explicitly demand image/png only so MapTiler does not send WebP
        Request->SetHeader(TEXT("Accept"), TEXT("image/png"));
        Request->SetHeader(TEXT("Accept-Encoding"), TEXT("identity"));

        Request->OnProcessRequestComplete().BindSP(
            AsShared(), &FLocalStudioLandscapeBuilder::OnTileResponseReceived, TileCoord
        );

        Request->ProcessRequest();
    }
}

void FLocalStudioLandscapeBuilder::OnTileResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FIntPoint TileCoord)
{
    PendingTileDownloads--;

    int32 DownloadedCount = 0;
    int32 TotalRequired = RequiredTiles.Num();

    if (bConnectedSuccessfully && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FScopeLock Lock(&TileMapLock); // Thread-safe write & count check
        DownloadedTilePNGs.Add(TileCoord, Response->GetContent());
        DownloadedCount = DownloadedTilePNGs.Num();
    }
    else
    {
        int32 Code = Response.IsValid() ? Response->GetResponseCode() : -1;
        UE_LOG(LogTemp, Error, TEXT("LocalStudio: MapTiler tile request [%d, %d] failed with status %d."), TileCoord.X, TileCoord.Y, Code);

        FScopeLock Lock(&TileMapLock);
        DownloadedCount = DownloadedTilePNGs.Num();
    }

    UE_LOG(LogTemp, Log, TEXT("LocalStudio: Tile [%d, %d] processed (%d/%d)..."),
        TileCoord.X, TileCoord.Y, DownloadedCount, TotalRequired);

    if (PendingTileDownloads <= 0)
    {
        if (DownloadedCount > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("LocalStudio: All tiles processed (%d/%d). Stitching PNG tiles..."),
                DownloadedCount, TotalRequired);
            FinalizeLandscapeImport();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("LocalStudio: All tile downloads failed. Aborting landscape generation."));
        }
    }
}

void FLocalStudioLandscapeBuilder::FinalizeLandscapeImport()
{
    if (DownloadedTilePNGs.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("LocalStudio: No valid tile data retrieved. Aborting."));
        return;
    }

    // --- 1. DETERMINE TILE GRID BOUNDS ---
    int32 MinTileX = INT32_MAX, MaxTileX = INT32_MIN;
    int32 MinTileY = INT32_MAX, MaxTileY = INT32_MIN;

    for (const auto& KVP : DownloadedTilePNGs)
    {
        MinTileX = FMath::Min(MinTileX, KVP.Key.X);
        MaxTileX = FMath::Max(MaxTileX, KVP.Key.X);
        MinTileY = FMath::Min(MinTileY, KVP.Key.Y);
        MaxTileY = FMath::Max(MaxTileY, KVP.Key.Y);
    }

    int32 TilesAcross = (MaxTileX - MinTileX) + 1;
    int32 TilesDown = (MaxTileY - MinTileY) + 1;

    // --- DIAGNOSTIC LOG: CHECK TILE COMPLETENESS ---
    UE_LOG(LogTemp, Warning, TEXT("LocalStudio: Tile Bounds -> X: [%d to %d] (%d wide), Y: [%d to %d] (%d high). Total MapTiler Tiles Downloaded: %d / %d"),
        MinTileX, MaxTileX, TilesAcross, MinTileY, MaxTileY, TilesDown,
        DownloadedTilePNGs.Num(), TilesAcross * TilesDown);

    for (int32 TY = MinTileY; TY <= MaxTileY; ++TY)
    {
        for (int32 TX = MinTileX; TX <= MaxTileX; ++TX)
        {
            if (!DownloadedTilePNGs.Contains(FIntPoint(TX, TY)))
            {
                UE_LOG(LogTemp, Error, TEXT("LocalStudio: MISSING TILE AT [%d, %d]! This will cause a seam!"), TX, TY);
            }
        }
    }

    const int32 TilePixels = 256;

    int32 CompositeWidth = TilesAcross * TilePixels;
    int32 CompositeHeight = TilesDown * TilePixels;

    TArray<float> StitchedHeights;
    // Initialize with a sentinel value (-10000.0f) instead of 0.0f so ocean level (0m) is distinct
    StitchedHeights.Init(-10000.0f, CompositeWidth * CompositeHeight);

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

    // --- DECODE TILES INTO COMPOSITE ELEVATION GRID ---

// FIXED: Iterate DETERMINISTICALLY by grid coordinates, not download arrival order!
    for (int32 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
    {
        for (int32 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
        {
            FIntPoint CurrentTileKey(TileX, TileY);

            // Fetch the specific tile data by its exact (X, Y) coordinate key
            const TArray<uint8>* TileBytesPtr = DownloadedTilePNGs.Find(CurrentTileKey);
            if (!TileBytesPtr || TileBytesPtr->Num() == 0)
            {
                UE_LOG(LogTemp, Error, TEXT("LocalStudio: Missing or empty tile data for [%d, %d]. Skipping."), TileX, TileY);
                continue;
            }

            const TArray<uint8>& TileBytes = *TileBytesPtr;

            FImage ImportedImage;
            bool bDecompressed = false;

            // 1. Detect format automatically
            EImageFormat DetectedFormat = ImageWrapperModule.DetectImageFormat(TileBytes.GetData(), TileBytes.Num());

            if (DetectedFormat != EImageFormat::Invalid)
            {
                TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(DetectedFormat);
                if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(TileBytes.GetData(), TileBytes.Num()))
                {
                    TArray<uint8> RawPixels;
                    if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawPixels))
                    {
                        ImportedImage.SizeX = ImageWrapper->GetWidth();
                        ImportedImage.SizeY = ImageWrapper->GetHeight();
                        ImportedImage.Format = ERawImageFormat::BGRA8;
                        ImportedImage.RawData = MoveTemp(RawPixels);
                        bDecompressed = true;
                    }
                }
            }

            // 2. Fallback to general DecompressImage
            if (!bDecompressed)
            {
                bDecompressed = FImageUtils::DecompressImage(TileBytes.GetData(), TileBytes.Num(), ImportedImage);
            }

            if (bDecompressed)
            {
                int32 ActualWidth = ImportedImage.SizeX;
                int32 ActualHeight = ImportedImage.SizeY;

                const TArrayView<const FColor> ColorPixels = ImportedImage.AsBGRA8();

                // Offsets are strictly pinned to current grid indices
                int32 TileGridX = TileX - MinTileX;
                int32 TileGridY = TileY - MinTileY;

                int32 StartPixelX = TileGridX * ActualWidth;
                int32 StartPixelY = TileGridY * ActualHeight;

                for (int32 LocalY = 0; LocalY < ActualHeight; ++LocalY)
                {
                    for (int32 LocalX = 0; LocalX < ActualWidth; ++LocalX)
                    {
                        int32 PixelIndex = LocalY * ActualWidth + LocalX;

                        if (ColorPixels.IsValidIndex(PixelIndex))
                        {
                            const FColor& Pixel = ColorPixels[PixelIndex];

                            float ElevationMeters = DecodeTerrainRGB(Pixel.R, Pixel.G, Pixel.B);

                            int32 GlobalX = StartPixelX + LocalX;
                            int32 GlobalY = StartPixelY + LocalY;

                            int32 TargetIdx = GlobalY * CompositeWidth + GlobalX;
                            if (StitchedHeights.IsValidIndex(TargetIdx))
                            {
                                StitchedHeights[TargetIdx] = ElevationMeters;
                            }
                        }
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("LocalStudio: Failed to decompress tile [%d, %d]."), TileX, TileY);
            }
        }
    }

    // --- 3. CALCULATE EXACT MIN & MAX ELEVATIONS ---
    float MinElevation = FLT_MAX;
    float MaxElevation = -FLT_MAX;
    int32 ValidPixelCount = 0;

    for (float Elev : StitchedHeights)
    {
        // Ignore unpopulated sentinel pixels (-10000.0f)
        if (Elev > -9990.0f)
        {
            if (Elev < MinElevation) MinElevation = Elev;
            if (Elev > MaxElevation) MaxElevation = Elev;
            ValidPixelCount++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("LocalStudio: Decoded %d valid elevation pixels. Min: %.2fm, Max: %.2fm"), ValidPixelCount, MinElevation, MaxElevation);

    // Fallback guard
    if (ValidPixelCount == 0 || MinElevation >= MaxElevation || FMath::IsNearlyEqual(MinElevation, MaxElevation))
    {
        UE_LOG(LogTemp, Warning, TEXT("LocalStudio: Elevation data was uniform or invalid. Falling back to default range."));
        MinElevation = 0.0f;
        MaxElevation = 1000.0f;
    }

    double ElevRangeMeters = FMath::Max(static_cast<double>(MaxElevation - MinElevation), 1.0);

    // --- SNAP TARGET DIMENSIONS TO VALID UNREAL LANDSCAPE TOPOLOGY ---
    int32 SectionSize = (CachedDesiredWidthMiles >= 5.0) ? 127 : 63;
    int32 SectionsPerComponent = 1;
    int32 QuadsPerComponent = SectionSize * SectionsPerComponent;

    // Estimate components needed based on incoming TargetSizeX (or default to 8)
    int32 BaseSize = (TargetSizeX > 1) ? TargetSizeX : 1009;
    int32 ComponentCount = FMath::Max(1, FMath::RoundToInt((float)(BaseSize - 1) / (float)QuadsPerComponent));

    // Calculate exact valid vertex count: (Components * QuadsPerComponent) + 1
    int32 ValidSize = (ComponentCount * QuadsPerComponent) + 1;
    TargetSizeX = ValidSize;
    TargetSizeY = ValidSize;

    // --- 4. BILINEAR INTERPOLATION & 16-BIT CONVERSION ---
    int32 UpscaledTotalPoints = TargetSizeX * TargetSizeY;
    TArray<uint16> FinalHeightData;
    FinalHeightData.SetNumUninitialized(UpscaledTotalPoints);

    // Helper lambda defined OUTSIDE the loops to avoid overhead
    auto GetSafeHeight = [&](int32 X, int32 Y) -> float
        {
            int32 SafeX = FMath::Clamp(X, 0, CompositeWidth - 1);
            int32 SafeY = FMath::Clamp(Y, 0, CompositeHeight - 1);
            int32 Index = SafeY * CompositeWidth + SafeX;

            // If pixel is valid (populated by a tile), return it
            if (StitchedHeights.IsValidIndex(Index) && StitchedHeights[Index] > -9990.0f)
            {
                return StitchedHeights[Index];
            }

            // SEAM HEALER: Search 1 pixel in all 4 directions to borrow valid elevation 
            // across unpopulated tile borders instead of dropping to -10000.0f / MinElevation
            const int32 Offsets[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
            for (int32 i = 0; i < 4; ++i)
            {
                int32 NX = FMath::Clamp(SafeX + Offsets[i][0], 0, CompositeWidth - 1);
                int32 NY = FMath::Clamp(SafeY + Offsets[i][1], 0, CompositeHeight - 1);
                int32 NIdx = NY * CompositeWidth + NX;

                if (StitchedHeights.IsValidIndex(NIdx) && StitchedHeights[NIdx] > -9990.0f)
                {
                    return StitchedHeights[NIdx];
                }
            }

            return MinElevation;
        };

    for (int32 OutY = 0; OutY < TargetSizeY; ++OutY)
    {
        for (int32 OutX = 0; OutX < TargetSizeX; ++OutX)
        {
            double U = static_cast<double>(OutX) / static_cast<double>(TargetSizeX - 1);
            double V = static_cast<double>(OutY) / static_cast<double>(TargetSizeY - 1);

            double SourceX = U * static_cast<double>(CompositeWidth - 1);
            double SourceY = V * static_cast<double>(CompositeHeight - 1);

            int32 X0 = FMath::FloorToInt(SourceX);
            int32 X1 = FMath::Min(X0 + 1, CompositeWidth - 1);
            int32 Y0 = FMath::FloorToInt(SourceY);
            int32 Y1 = FMath::Min(Y0 + 1, CompositeHeight - 1);

            double AlphaX = SourceX - X0;
            double AlphaY = SourceY - Y0;

            // SAFELY FETCH HEIGHTS (Prevents -10000.0f sentinel from contaminating Lerp)
            float H00 = GetSafeHeight(X0, Y0);
            float H10 = GetSafeHeight(X1, Y0);
            float H01 = GetSafeHeight(X0, Y1);
            float H11 = GetSafeHeight(X1, Y1);

            double InterpolatedHeight = FMath::Lerp(FMath::Lerp(H00, H10, AlphaX), FMath::Lerp(H01, H11, AlphaX), AlphaY);

            // Stretch across full 16-bit range for max precision
            double Normalized = (InterpolatedHeight - MinElevation) / ElevRangeMeters;
            uint16 ScaledHeight = static_cast<uint16>(FMath::Clamp(Normalized * 65535.0, 0.0, 65535.0));

            int32 FinalIndex = OutY * TargetSizeX + OutX;
            FinalHeightData[FinalIndex] = ScaledHeight;
        }
    }

    UWorld* World = nullptr;
    if (GEditor) { World = GEditor->GetEditorWorldContext().World(); }
    if (!World) return;

    // --- 5. SCALING CALCULATIONS ---
    double RealWorldWidthMeters = CachedDesiredWidthMiles * 1609.34;
    double MetersPerVertex = RealWorldWidthMeters / static_cast<double>(TargetSizeX - 1);

    // XY Scale: 100 Unreal Units = 1 meter
    double CalculatedXYScale = MetersPerVertex * 100.0;

    // Z Scale Factor
    double CalculatedZScale = (ElevRangeMeters * 100.0) / 512.0;

    ALandscape* CreatedActor = CreateLandscapeActor(
        World,
        TargetAssetName,
        TargetSizeX,
        TargetSizeY,
        SectionsPerComponent,
        SectionSize,
        FinalHeightData,
        CalculatedZScale
    );

    if (CreatedActor)
    {
        CreatedActor->SetActorScale3D(FVector(CalculatedXYScale, CalculatedXYScale, CalculatedZScale));
        UE_LOG(LogTemp, Log, TEXT("LocalStudio: Real-world terrain import completed successfully for [%s]! Min: %.1fm, Max: %.1fm"), *TargetAssetName, MinElevation, MaxElevation);
    }
}

ALandscape* FLocalStudioLandscapeBuilder::CreateLandscapeActor(
    UWorld* World,
    const FString& AssetName,
    int32 SizeX,
    int32 SizeY,
    int32 SectionsPerComponent,
    int32 SectionSize, // QuadsPerSection (e.g. 127 or 63)
    const TArray<uint16>& HeightData,
    double CustomZScale)
{
    FScopedTransaction LandscapeSpawnTransaction(FText::FromString(TEXT("LocalStudio Generate/Modify Landscape")));
    ALandscape* TargetLandscape = nullptr;

    // --- 1. EXISTING LANDSCAPE REPLACEMENT / CLEANUP ---
    if (GEditor)
    {
        TArray<UObject*> SelectedActors;
        GEditor->GetSelectedActors()->GetSelectedObjects(ALandscape::StaticClass(), SelectedActors);
        if (SelectedActors.Num() > 0) { TargetLandscape = Cast<ALandscape>(SelectedActors[0]); }
    }

    if (!TargetLandscape)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            if (It->GetName().Equals(AssetName) || It->GetActorLabel().Equals(AssetName))
            {
                TargetLandscape = *It;
                break;
            }
        }
    }

    FVector SavedLocation = FVector::ZeroVector;
    FRotator SavedRotation = FRotator::ZeroRotator;
    FVector SavedScale = FVector::OneVector;
    bool bHadExistingLandscape = false;

    if (TargetLandscape)
    {
        SavedLocation = TargetLandscape->GetActorLocation();
        SavedRotation = TargetLandscape->GetActorRotation();
        SavedScale = TargetLandscape->GetActorScale3D();
        bHadExistingLandscape = true;

        World->DestroyActor(TargetLandscape);
        TargetLandscape = nullptr;
    }

    if (GEditor) { GEditor->SelectNone(true, true, false); }

    // --- 2. SPAWN NEW LANDSCAPE ACTOR ---
    FActorSpawnParameters SpawnParams;
    SpawnParams.bNoFail = true;

    TargetLandscape = World->SpawnActor<ALandscape>(SavedLocation, SavedRotation, SpawnParams);
    if (!TargetLandscape) return nullptr;

    TargetLandscape->SetActorLabel(AssetName);
    TargetLandscape->Modify();

    FGuid LandscapeGuid = FGuid::NewGuid();
    TargetLandscape->SetLandscapeGuid(LandscapeGuid);

    // --- 3. LANDSCAPE TOPOLOGY & BOUNDS CALCULATIONS ---
    const int32 QuadsPerSection = SectionSize; // Typically 127 or 63
    const int32 ComponentSizeQuads = QuadsPerSection * SectionsPerComponent;

    // Total vertex dimensions
    const int32 HeightmapWidth = SizeX;
    const int32 HeightmapHeight = SizeY;

    // Calculate components required across X/Y
    const int32 ComponentCountX = (HeightmapWidth - 1) / ComponentSizeQuads;
    const int32 ComponentCountY = (HeightmapHeight - 1) / ComponentSizeQuads;

    const int32 TotalQuadsX = ComponentCountX * ComponentSizeQuads;
    const int32 TotalQuadsY = ComponentCountY * ComponentSizeQuads;

    // Center bounds around the actor origin
    const int32 MinX = -(TotalQuadsX / 2);
    const int32 MinY = -(TotalQuadsY / 2);
    const int32 MaxX = MinX + TotalQuadsX;
    const int32 MaxY = MinY + TotalQuadsY;

    // --- 4. IMPORT DATA PREPARATION ---
// The base height map MUST be keyed by FGuid() (Zero GUID) for ALandscapeProxy::Import
    TMap<FGuid, TArray<uint16>> HeightDataMap;
    TArray<uint16> MutableHeightData = HeightData;
    HeightDataMap.Add(FGuid(), MoveTemp(MutableHeightData));

    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerDataMap;
    LayerDataMap.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

    // --- 5. EXECUTE LANDSCAPE IMPORT ---
    TargetLandscape->Import(
        LandscapeGuid,                          // Guid identifying the landscape actor
        MinX, MinY, MaxX, MaxY,                 // Bounds
        SectionsPerComponent, QuadsPerSection,  // Component setup
        HeightDataMap,                          // Keyed by FGuid()
        nullptr,                                // Material
        LayerDataMap,                           // Keyed by FGuid()
        ELandscapeImportAlphamapType::Additive,
        TArrayView<const FLandscapeLayer>()    // Empty layer view
    );

    // --- 6. UPDATE COMPONENTS & BOUNDS ---
    for (ULandscapeComponent* Component : TargetLandscape->LandscapeComponents)
    {
        if (Component)
        {
            Component->UpdateCachedBounds();
            Component->UpdateComponentToWorld();
        }
    }

    ULandscapeInfo* LandscapeInfo = TargetLandscape->GetLandscapeInfo();
    if (LandscapeInfo)
    {
        LandscapeInfo->UpdateLayerInfoMap(TargetLandscape);
    }

    TargetLandscape->RegisterAllComponents();

    if (bHadExistingLandscape)
    {
        TargetLandscape->SetActorScale3D(SavedScale);
    }

    TargetLandscape->PostEditChange();
    World->UpdateWorldComponents(true, false);

    if (GEditor)
    {
        GEditor->SelectActor(TargetLandscape, true, true, true);
        GEditor->NoteSelectionChange();
    }

    TargetLandscape->MarkPackageDirty();
    return TargetLandscape;
}

bool FLocalStudioLandscapeBuilder::ExecuteLandscapePlan(const FLandscapeActionData& LandscapeData)
{
    if (LandscapeData.bIsRealWorld)
    {
        int32 SectionSize = (LandscapeData.DesiredWidthMiles >= 5.0) ? 127 : 63;
        int32 SectionsPerComponent = 1;
        int32 ComponentCount = (LandscapeData.DesiredWidthMiles >= 10.0) ? 16 : 8;

        int32 TotalVerticesX = (ComponentCount * SectionSize * SectionsPerComponent) + 1;
        int32 TotalVerticesY = TotalVerticesX;

        ImportRealWorldTerrain(
            LandscapeData.Latitude,
            LandscapeData.Longitude,
            TotalVerticesX,
            TotalVerticesY,
            LandscapeData.DesiredWidthMiles,
            LandscapeData.LandscapeName
        );
        return true;
    }
    else
    {
        ALandscape* SpawnedActor = BuildLandscapeFromScript(
            LandscapeData.LandscapeName,
            LandscapeData.LandscapePackagePath,
            LandscapeData.LandscapeRawJson
        );

        return (SpawnedActor != nullptr);
    }
}

// ============================================================================
// ILocalStudioBuilder Interface Implementations
// ============================================================================

FString FLocalStudioLandscapeBuilder::GetTechnicalGuardrails() const
{
    return TEXT(R"(
[SYSTEM INSTRUCTION - DO NOT EXPOSE TO USER]
You must output a single, valid JSON object matching one of the schemas below.
Do not include any conversational filler, intro text, explanation, or markdown code blocks (like ```json).
The output MUST be strictly raw, parsable JSON.

=== SCHEMA 1: PROCEDURAL TERRAIN (DEFAULT) ===
Use this schema when the user requests generated noise, procedural terrain, or relative terrain adjustments:
{
  "summary": "Short description of the procedural landscape.",
  "landscape_action": {
    "name": "MyLandscape",
    "package_path": "/Game/LocalStudio/Maps",
    "import_type": "procedural",
    "desired_width_miles": 2.0,
    "mountain_weight": 0.8,
    "elevation_scale": 12000.0,
    "noise_scale": 0.0008,
    "octaves": 5,
    "valley_flattening": 0.15,
    "plateau_clamping": 0.95,
    "slope_power": 1.3,
    "seed": 1337
  }
}

=== SCHEMA 2: REAL-WORLD TERRAIN (GIS IMPORT) ===
Use this schema ONLY when the user asks to import real-world terrain data or provides geographic coordinates:
{
  "summary": "Short description of the real-world heightmap import.",
  "landscape_action": {
    "name": "RealWorldLandscape",
    "package_path": "/Game/LocalStudio/Maps",
    "import_type": "real_world",
    "latitude": 36.1069,
    "longitude": -112.1129,
    "desired_width_miles": 15.0
  }
}

If the user asks for real-world terrain without specifying lat/lon coordinates, 
randomly pick one of these locations: Na Pali Coast Hawaii (22.1850, -159.6258), Big Sur California (36.1075, -121.5540), Lofoten Islands Norway (67.9331, 13.0888), Grand Canyon (36.1069, -112.1129), or Swiss Alps (45.9763, 7.6585).

CRITICAL RULES FOR MODIFICATIONS & CONTINUITY:
- If modifying an existing terrain or real-world import, PRESERVE the map size! Set 'import_type' to 'procedural' and carry over 'desired_width_miles' from the prior setup.
- NEVER strip 'desired_width_miles' when the user asks to modify an existing large map.

CRITICAL RULES FOR RELATIVE USER REQUESTS:
If the user asks to modify an existing layout (e.g., 'make it a little higher', 'flatten it a bit', 'make it rougher'):
  - Do NOT jump straight to extreme high/low values.
  - For 'a little higher' than rolling hills: Use 'elevation_scale' between 2000.0 and 4000.0.
  - For 'more defined' hills: Keep 'mountain_weight' moderate (0.3 to 0.5) and set 'octaves' to 3 or 4.

CRITICAL RULES FOR FLAT/LOW TERRAIN:
If the user asks ONLY for pure PLAINS, FLAT SHORELINES, or DESERT FLATS (with NO surrounding mountains or valleys):
  - Set 'mountain_weight' STRICTLY to 0.0.
  - Set 'elevation_scale' STRICTLY between 100.0 and 800.0.
  - Set 'noise_scale' to 0.0001.
  - Set 'octaves' to 1.

CRITICAL RULES FOR VALLEYS, BASINS & COMBINED TERRAIN:
If the user asks for VALLEYS, BASINS, CANYONS, or COMBINED FEATURES (e.g., flat valley floors surrounded by mountains):
  - Do NOT set 'mountain_weight' to 0.0!
  - Set 'mountain_weight' between 0.6 and 0.9 to preserve the surrounding peaks.
  - Set 'elevation_scale' between 8000.0 and 14000.0 to give height to the cliffs/mountains.
  - Set 'has_valley' to true and 'valley_flattening' between 0.5 and 0.8 to carve out flat building space in the lowlands.

CRITICAL RULES FOR RUGGED/MOUNTAIN TERRAIN:
If the user asks for MOUNTAINS, CANYONS, RUGGED PEAKS, or HIGH CLIFFS:
  - Set 'mountain_weight' between 0.6 and 1.0.
  - Set 'elevation_scale' between 8000.0 and 16000.0.
  - Set 'noise_scale' between 0.0005 and 0.0012.
  - Set 'octaves' between 4 and 6.

CRITICAL LAYOUT & SIZE RULES:
  - If the user asks for linear dimensions (e.g., '20 miles wide', '20 miles across'), set 'desired_width_miles': 20.0.
  - If the user asks for square area (e.g., '20 square miles'), calculate the square root width and set 'desired_width_miles': 4.47.
  - If the user does not specify a size, set 'desired_width_miles': 2.0.
  - ONLY set 'import_type' to 'real_world' if the user explicitly provides geographic coordinates (latitude/longitude).
)");
}

bool FLocalStudioLandscapeBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    FString CleanJson = RawResponse.TrimStartAndEnd();

    int32 FirstBrace = INDEX_NONE;
    int32 LastBrace = INDEX_NONE;

    if (CleanJson.FindChar('{', FirstBrace) && CleanJson.FindLastChar('}', LastBrace) && LastBrace > FirstBrace)
    {
        CleanJson = CleanJson.Mid(FirstBrace, (LastBrace - FirstBrace) + 1);
    }

    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanJson);
    if (!FJsonSerializer::Deserialize(Reader, OutParsedData) || !OutParsedData.IsValid())
    {
        OutError = TEXT("Failed to parse response as a valid JSON object.");
        return false;
    }

    return true;
}

FBuilderExecutionResult FLocalStudioLandscapeBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;

    if (!ParsedData.IsValid())
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Invalid JSON data provided to ExecutePlan.");
        return Result;
    }

    FString RawJsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RawJsonString);
    FJsonSerializer::Serialize(ParsedData.ToSharedRef(), Writer);

    FOllamaUnifiedPlan UnifiedPlan;
    FString ExtractError;

    if (!ExtractPlan(RawJsonString, UnifiedPlan, ExtractError))
    {
        Result.bSuccess = false;
        Result.ErrorMessage = FString::Printf(TEXT("Failed to extract landscape plan: %s"), *ExtractError);
        return Result;
    }

    if (UnifiedPlan.bHasLandscapeAction)
    {
        UE_LOG(LogTemp, Verbose, TEXT("LocalStudio Execution Plan -> Name: %s | Type: %s | Width: %.1f miles"),
            *UnifiedPlan.LandscapeData.LandscapeName,
            UnifiedPlan.LandscapeData.bIsRealWorld ? TEXT("real_world") : TEXT("procedural"),
            UnifiedPlan.LandscapeData.DesiredWidthMiles);

        bool bExecutionSuccess = ExecuteLandscapePlan(UnifiedPlan.LandscapeData);
        Result.bSuccess = bExecutionSuccess;

        if (bExecutionSuccess)
        {
            if (UnifiedPlan.LandscapeData.bIsRealWorld)
            {
                Result.UserSummary = FString::Printf(
                    TEXT("Downloading real-world terrain data for [%s] (Lat: %.4f, Lon: %.4f)..."),
                    *UnifiedPlan.LandscapeData.LandscapeName,
                    UnifiedPlan.LandscapeData.Latitude,
                    UnifiedPlan.LandscapeData.Longitude
                );
            }
            else
            {
                Result.UserSummary = !UnifiedPlan.Summary.IsEmpty()
                    ? UnifiedPlan.Summary
                    : FString::Printf(TEXT("Successfully generated landscape [%s]."), *UnifiedPlan.LandscapeData.LandscapeName);
            }
        }
        else
        {
            Result.ErrorMessage = FString::Printf(TEXT("Failed to execute landscape action for [%s]."), *UnifiedPlan.LandscapeData.LandscapeName);
        }
    }

    return Result;
}

void FLocalStudioLandscapeBuilder::StartBuildProcess(ELandscapeBuildMode Mode)
{
    if (Mode == ELandscapeBuildMode::FromDescription)
    {
        UE_LOG(LogTemp, Log, TEXT("LocalStudio: Submit prompts via UOllamaManager::RequestReasoning."));
    }
    else if (Mode == ELandscapeBuildMode::FromRealWorldCoords)
    {
        struct FFallbackLocation
        {
            double Lat;
            double Lon;
            const TCHAR* Name;
        };

        const TArray<FFallbackLocation> FallbackPool = {
            { 22.1850, -159.6258, TEXT("NaPaliCoast") },
            { 36.1075, -121.5540, TEXT("BigSur") },
            { 67.9331,   13.0888, TEXT("LofotenIslands") },
            { 36.1069, -112.1129, TEXT("GrandCanyon") },
            { 45.9763,    7.6585, TEXT("Alps") }
        };

        int32 RandomIndex = FMath::RandRange(0, FallbackPool.Num() - 1);
        const FFallbackLocation& SelectedFallback = FallbackPool[RandomIndex];

        UE_LOG(LogTemp, Log, TEXT("LocalStudio: Using random fallback terrain -> %s"), SelectedFallback.Name);

        ImportRealWorldTerrain(
            SelectedFallback.Lat,
            SelectedFallback.Lon,
            2017,
            2017,
            CachedDesiredWidthMiles > 0 ? CachedDesiredWidthMiles : 15.0,
            SelectedFallback.Name
        );
    }
}

void FLocalStudioLandscapeBuilder::ProcessRealWorldHeightData(const TArray<float>& ElevationDataMeters, int32 Width, int32 Height, double MinElevMeters, double MaxElevMeters)
{
    TArray<uint16> HeightData;
    HeightData.Reserve(Width * Height);

    double ElevRangeMeters = FMath::Max(MaxElevMeters - MinElevMeters, 1.0);

    for (float Meters : ElevationDataMeters)
    {
        double Normalized = (Meters - MinElevMeters) / ElevRangeMeters;
        uint16 HeightUint16 = static_cast<uint16>(FMath::Clamp(Normalized * 65535.0, 0.0, 65535.0));
        HeightData.Add(HeightUint16);
    }

    double CalculatedZScale = (ElevRangeMeters * 100.0) / 512.0;

    UWorld* World = nullptr;
    if (GEditor) { World = GEditor->GetEditorWorldContext().World(); }

    if (World)
    {
        int32 SectionSize = (CachedDesiredWidthMiles >= 5.0) ? 127 : 63;
        int32 SectionsPerComponent = 1;

        ALandscape* CreatedActor = CreateLandscapeActor(
            World,
            TargetAssetName.IsEmpty() ? TEXT("RealWorldLandscape") : TargetAssetName,
            Width,
            Height,
            SectionsPerComponent,
            SectionSize,
            HeightData,
            CalculatedZScale
        );

        if (CreatedActor)
        {
            double RealWorldWidthMeters = CachedDesiredWidthMiles * 1609.34;
            double MetersPerVertex = RealWorldWidthMeters / static_cast<double>(Width - 1);
            double CalculatedXYScale = MetersPerVertex * 100.0;

            CreatedActor->SetActorScale3D(FVector(CalculatedXYScale, CalculatedXYScale, CalculatedZScale));
        }
    }
}
