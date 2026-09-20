#pragma once

// Editor Utilities
#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "Engine/AssetManager.h"
#include "UObject/ObjectRedirector.h"

// Material Headers
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"


struct FLocalStudioAssetUtils
{
    /**
     * Searches for an asset by name anywhere in the project, resolving redirectors and handling moved assets.
     */
    template <typename TAssetType>
    static TAssetType* FindAssetByName(const FString& InAssetName)
    {
        if (InAssetName.IsEmpty()) return nullptr;

        // Clean up input string (strip paths/extensions if provided)
        FString CleanAssetName = FPaths::GetBaseFilename(InAssetName);

        // Tier 1: Check if input was already a valid object path (e.g. "/Game/LocalStudio/Materials/M_BaseMaterial")
        if (InAssetName.StartsWith(TEXT("/")))
        {
            if (TAssetType* DirectAsset = LoadObject<TAssetType>(nullptr, *InAssetName))
            {
                return DirectAsset;
            }
        }

        // Tier 2: Check if asset is already loaded in memory anywhere in /Game/
        for (TObjectIterator<TAssetType> It; It; ++It)
        {
            TAssetType* LoadedObj = *It;
            if (LoadedObj && LoadedObj->GetName().Equals(CleanAssetName, ESearchCase::IgnoreCase))
            {
                // Ensure it belongs to the main content folder /Game/
                if (LoadedObj->GetPathName().StartsWith(TEXT("/Game/")))
                {
                    return LoadedObj;
                }
            }
        }

        // Tier 3: Scan AssetRegistry across all of /Game/
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        if (AssetRegistry.IsLoadingAssets())
        {
            AssetRegistry.SearchAllAssets(true);
        }

        TArray<FAssetData> AssetDataList;

        FARFilter Filter;
        // Search broadly under UObject or UMaterialInterface if applicable
        if (TAssetType::StaticClass()->IsChildOf(UMaterialInterface::StaticClass()))
        {
            Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
        }
        else
        {
            Filter.ClassPaths.Add(TAssetType::StaticClass()->GetClassPathName());
        }

        Filter.bRecursiveClasses = true;
        Filter.PackagePaths.Add(TEXT("/Game"));

        AssetRegistry.GetAssets(Filter, AssetDataList);

        for (const FAssetData& AssetData : AssetDataList)
        {
            if (AssetData.AssetName.ToString().Equals(CleanAssetName, ESearchCase::IgnoreCase))
            {
                UObject* LoadedAsset = AssetData.GetAsset();
                if (!LoadedAsset) continue;

                // Resolve Unreal Redirectors if the asset was moved in Content Browser
                if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadedAsset))
                {
                    LoadedAsset = Redirector->DestinationObject;
                }

                if (TAssetType* ResolvedAsset = Cast<TAssetType>(LoadedAsset))
                {
                    return ResolvedAsset;
                }
            }
        }

        // Tier 4: Fallback - Check root /Game/ directly with LoadObject
        FString RootPath = FString::Printf(TEXT("/Game/%s.%s"), *CleanAssetName, *CleanAssetName);
        if (TAssetType* FallbackAsset = LoadObject<TAssetType>(nullptr, *RootPath))
        {
            return FallbackAsset;
        }

        return nullptr;
    }

    /**
     * Finds an existing asset anywhere in the project, or creates a new one at DefaultFolder if missing.
     * Returns true in bOutWasCreated if a brand new asset was spawned on disk.
     */
    template <typename TAssetType>
    static TAssetType* FindOrCreateAsset(
        const FString& AssetName,
        const FString& DefaultFolder,
        UFactory* Factory,
        bool& bOutWasCreated)
    {
        bOutWasCreated = false;

        // 1. Search ENTIRE project dynamically
        TAssetType* ExistingAsset = FindAssetByName<TAssetType>(AssetName);
        if (ExistingAsset)
        {
            UE_LOG(LogTemp, Log, TEXT("LocalStudio: Reusing existing asset [%s] at [%s]"), *AssetName, *ExistingAsset->GetPathName());
            return ExistingAsset;
        }

        // 2. Not found - create new asset at DefaultFolder
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        TAssetType* NewAsset = Cast<TAssetType>(AssetToolsModule.Get().CreateAsset(
            AssetName,
            DefaultFolder,
            TAssetType::StaticClass(),
            Factory
        ));

        if (NewAsset)
        {
            bOutWasCreated = true;
            UE_LOG(LogTemp, Log, TEXT("LocalStudio: Created new asset [%s] at [%s]"), *AssetName, *NewAsset->GetPathName());
        }

        return NewAsset;
    }

    template<typename T>
    static T* FindOrCreateAssetWithPolicy(
        const FString& InAssetName,
        const FString& PreferredPackagePath,
        UFactory* Factory,
        bool& bOutWasCreated,
        bool bAllowOverwrite = true,
        FString* OutActualName = nullptr
    );

    /** Helper to generate a non-conflicting unique name (e.g., M_Flasher -> M_Flasher_01) */
    static inline FString GetUniqueAssetName(const FString& BaseName, const FString& PackagePath);
};

// Generate a unique name if an asset exists at PackagePath OR anywhere in the Asset Registry
inline FString FLocalStudioAssetUtils::GetUniqueAssetName(const FString& BaseName, const FString& PackagePath)
{
    FString TargetName = BaseName;
    int32 Index = 1;

    // Keep checking if the asset name exists project-wide
    while (FindAssetByName<UObject>(TargetName) != nullptr)
    {
        TargetName = FString::Printf(TEXT("%s_%02d"), *BaseName, Index);
        Index++;
    }

    return TargetName;
}

template<typename T>
T* FLocalStudioAssetUtils::FindOrCreateAssetWithPolicy(
    const FString& InAssetName,
    const FString& PreferredPackagePath,
    UFactory* Factory,
    bool& bOutWasCreated,
    bool bAllowOverwrite,
    FString* OutActualName)
{
    bOutWasCreated = false;
    FString FinalAssetName = InAssetName;

    // 1. Check if the asset already exists anywhere in the project (handles moved assets)
    T* ExistingAsset = FindAssetByName<T>(InAssetName);

    if (ExistingAsset)
    {
        if (bAllowOverwrite)
        {
            UE_LOG(LogTemp, Log, TEXT("LocalStudio: Found existing asset '%s' at '%s'. Updating in-place..."),
                *InAssetName, *ExistingAsset->GetPathName());

            // If it's a Material, clear expression nodes so we start with a clean graph slate
            if (UMaterial* Mat = Cast<UMaterial>(ExistingAsset))
            {
                Mat->GetExpressionCollection().Empty();
            }

            if (OutActualName) *OutActualName = InAssetName;
            return ExistingAsset;
        }
        else
        {
            // Auto-increment to create a duplicate version without destroying the original
            FinalAssetName = GetUniqueAssetName(InAssetName, PreferredPackagePath);
        }
    }

    // 2. Create package and asset using the resolved name
    FString FullPackagePath = PreferredPackagePath / FinalAssetName;
    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package) return nullptr;

    T* NewAsset = nullptr;
    if (Factory)
    {
        NewAsset = Cast<T>(Factory->FactoryCreateNew(
            T::StaticClass(), Package, FName(*FinalAssetName),
            RF_Public | RF_Standalone, nullptr, GWarn
        ));
    }

    if (NewAsset)
    {
        bOutWasCreated = true;
        FAssetRegistryModule::AssetCreated(NewAsset);
        NewAsset->MarkPackageDirty();
    }

    if (OutActualName) *OutActualName = FinalAssetName;
    return NewAsset;
}