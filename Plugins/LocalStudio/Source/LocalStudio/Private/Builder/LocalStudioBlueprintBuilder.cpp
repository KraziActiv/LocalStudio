// LocalStudioBlueprintBuilder.cpp
#include "Builder/LocalStudioBlueprintBuilder.h"
#include "Core/LocalStudioAssetUtils.h"
#include "Core/LocalStudioJsonUtils.h"

#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#if WITH_EDITOR
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetMathLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SavePackage.h"
#include "EdGraphSchema_K2.h"

#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_CallFunction.h"
#include "K2Node_PromotableOperator.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ChildActorComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#endif

namespace
{
    static EBlueprintType ResolveBlueprintTypeFromString(const FString& RawType)
    {
        FString Normalized = RawType.TrimStartAndEnd().ToLower();

        if (Normalized.Contains(TEXT("interface")))
        {
            return EBlueprintType::BPTYPE_Interface;
        }
        if (Normalized.Contains(TEXT("macro")))
        {
            return EBlueprintType::BPTYPE_MacroLibrary;
        }
        if (Normalized.Contains(TEXT("function_library")) || Normalized.Contains(TEXT("functionlibrary")) || Normalized.Contains(TEXT("function library")))
        {
            return EBlueprintType::BPTYPE_FunctionLibrary;
        }
        if (Normalized.Contains(TEXT("widget")))
        {
            return EBlueprintType::BPTYPE_Normal;
        }
        if (Normalized.Contains(TEXT("anim")))
        {
            return EBlueprintType::BPTYPE_Normal;
        }
        if (Normalized.Contains(TEXT("levelscript")) || Normalized.Contains(TEXT("level_script")))
        {
            return EBlueprintType::BPTYPE_LevelScript;
        }
        if (Normalized.Contains(TEXT("editorutility")) || Normalized.Contains(TEXT("editor_utility")))
        {
            return EBlueprintType::BPTYPE_EditorUtilityBlueprint;
        }

        return EBlueprintType::BPTYPE_Normal;
    }
}

FBuilderExecutionResult FLocalStudioBlueprintBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = false;

    if (!ParsedData.IsValid())
    {
        Result.UserSummary = TEXT("Invalid Blueprint JSON payload.");
        return Result;
    }

    const TArray<TSharedPtr<FJsonValue>>* BlueprintsArray = nullptr;
    if (ParsedData->TryGetArrayField(TEXT("blueprints"), BlueprintsArray) && BlueprintsArray && BlueprintsArray->Num() > 0)
    {
        FString CombinedSummaries;
        bool bAllSucceeded = true;

        for (const TSharedPtr<FJsonValue>& BPValue : *BlueprintsArray)
        {
            if (!BPValue.IsValid() || BPValue->Type != EJson::Object)
            {
                continue;
            }

            FBuilderExecutionResult SingleResult = ExecutePlan(BPValue->AsObject());
            if (!SingleResult.bSuccess)
            {
                bAllSucceeded = false;
            }
            if (!SingleResult.UserSummary.IsEmpty())
            {
                CombinedSummaries += SingleResult.UserSummary + TEXT("\n\n");
            }
        }

        Result.bSuccess = bAllSucceeded;
        Result.UserSummary = CombinedSummaries.TrimEnd();
        return Result;
    }

    FString BlueprintName = ParsedData->HasField(TEXT("blueprint_name")) ? ParsedData->GetStringField(TEXT("blueprint_name")) : TEXT("BP_NewBlueprint");
    FString ParentClassName = ParsedData->HasField(TEXT("parent_class")) ? ParsedData->GetStringField(TEXT("parent_class")) : TEXT("Actor");
    FString BlueprintTypeName = ParsedData->HasField(TEXT("blueprint_type")) ? ParsedData->GetStringField(TEXT("blueprint_type")) : TEXT("Normal");
    FString PackagePath = ParsedData->HasField(TEXT("package_path")) ? ParsedData->GetStringField(TEXT("package_path")) : TEXT("/Game/LocalStudio/Blueprints");

    if (ParentClassName.Equals(TEXT("Object"), ESearchCase::IgnoreCase) || ParentClassName.Equals(TEXT("UObject"), ESearchCase::IgnoreCase))
    {
        ParentClassName = TEXT("Actor");
    }

    if (!BlueprintName.StartsWith(TEXT("BP_")))
    {
        BlueprintName = TEXT("BP_") + BlueprintName;
    }

    UClass* ParentClass = ResolveParentClass(ParentClassName);
    if (!ParentClass)
    {
        Result.UserSummary = FString::Printf(TEXT("Failed to resolve parent class '%s'."), *ParentClassName);
        return Result;
    }

    EBlueprintType BlueprintType = ResolveBlueprintTypeFromString(BlueprintTypeName);

#if WITH_EDITOR
    UBlueprint* ExistingBlueprint = FLocalStudioAssetUtils::FindAssetByName<UBlueprint>(BlueprintName);
    UBlueprint* NewBlueprint = ExistingBlueprint;
    bool bCreatedBlueprint = false;

    if (!ExistingBlueprint)
    {
        FString FullPackagePath = PackagePath;
        if (!FullPackagePath.StartsWith(TEXT("/Game/")))
        {
            FullPackagePath = TEXT("/Game/") + FullPackagePath;
        }

        if (!FullPackagePath.EndsWith(TEXT("/")))
        {
            FullPackagePath += TEXT("/");
        }

        FString PackageString = FullPackagePath + BlueprintName;

        UPackage* Package = CreatePackage(*PackageString);
        if (!Package)
        {
            Result.UserSummary = FString::Printf(TEXT("Failed to create package for blueprint '%s' at '%s'."), *BlueprintName, *PackageString);
            return Result;
        }

        NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
            ParentClass,
            Package,
            FName(*BlueprintName),
            BlueprintType,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass()
        );

        if (!NewBlueprint)
        {
            Result.UserSummary = FString::Printf(TEXT("Failed to create Blueprint asset '%s' with parent '%s'."), *BlueprintName, *ParentClassName);
            return Result;
        }

        bCreatedBlueprint = true;
        FAssetRegistryModule::AssetCreated(NewBlueprint);
    }

    if (!NewBlueprint)
    {
        Result.UserSummary = FString::Printf(TEXT("Blueprint '%s' could not be created or found."), *BlueprintName);
        return Result;
    }

    int32 VariablesAdded = 0;
    int32 VariablesSkipped = 0;
    int32 VariablesAlreadyExisting = 0;
    int32 FunctionsAdded = 0;
    int32 FunctionsSkipped = 0;
    int32 FunctionsAlreadyExisting = 0;
    int32 ComponentsAdded = 0;
    int32 ComponentsSkipped = 0;
    int32 ComponentsAlreadyExisting = 0;

    if (!ProcessComponents(NewBlueprint, ParsedData, ParentClass, ComponentsAdded, ComponentsSkipped, ComponentsAlreadyExisting))
    {
        Result.UserSummary = TEXT("Failed while processing Blueprint components.");
        return Result;
    }

    if (!ProcessVariables(NewBlueprint, ParsedData, ParentClass, VariablesAdded, VariablesSkipped, VariablesAlreadyExisting))
    {
        Result.UserSummary = TEXT("Failed while processing Blueprint variables.");
        return Result;
    }

    if (!ProcessFunctions(NewBlueprint, ParsedData, ParentClass, FunctionsAdded, FunctionsSkipped, FunctionsAlreadyExisting))
    {
        Result.UserSummary = TEXT("Failed while processing Blueprint functions.");
        return Result;
    }

    NewBlueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    FString AssetPath = NewBlueprint->GetOutermost()->GetName();
    FString PackageFilename = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GWarn;
    SaveArgs.bForceByteSwapping = false;

    if (!UPackage::SavePackage(NewBlueprint->GetOutermost(), NewBlueprint, *PackageFilename, SaveArgs))
    {
        Result.UserSummary = FString::Printf(TEXT("Blueprint was created but failed to save: %s"), *AssetPath);
        return Result;
    }

    Result.bSuccess = true;

    FString BlueprintAction = bCreatedBlueprint ? TEXT("created") : TEXT("updated");
    Result.UserSummary = FString::Printf(
        TEXT("Successfully %s Blueprint [%s] at %s.\n")
        TEXT("Components: Added=%d Existing=%d Skipped=%d\n")
        TEXT("Variables: Added=%d Existing=%d Skipped=%d\n")
        TEXT("Functions: Added=%d Existing=%d Skipped=%d\n")
        TEXT("Blueprint Type: %s\n")
        TEXT("Parent Class: %s"),
        *BlueprintAction,
        *BlueprintName,
        *AssetPath,
        ComponentsAdded,
        ComponentsAlreadyExisting,
        ComponentsSkipped,
        VariablesAdded,
        VariablesAlreadyExisting,
        VariablesSkipped,
        FunctionsAdded,
        FunctionsAlreadyExisting,
        FunctionsSkipped,
        *UEnum::GetValueAsString(BlueprintType),
        *ParentClassName
    );

    return Result;
#else
    Result.UserSummary = TEXT("Blueprint generation is editor-only and is unavailable outside WITH_EDITOR builds.");
    return Result;
#endif
}

FString FLocalStudioBlueprintBuilder::GetTechnicalGuardrails() const
{
    return TEXT(R"raw(
You are LocalStudio, an Unreal Engine 5.7 Blueprint development assistant.
Your job is to produce a strict JSON intermediate representation that LocalStudio can translate into an Unreal Engine Blueprint.

Core rules:
- Support all Blueprint asset kinds: normal Blueprint, Interface, Function Library, Macro Library, Widget, and other standard Unreal Blueprint families.
- Implement only what the user explicitly requested.
- Default to a safe normal Blueprint when the requested type is not clear.
- Respect the parent class requested by the user or the engine default if unspecified.
- Do not invent speculative assets or graph logic.
- Use the smallest valid Blueprint structure.
- If a requested Blueprint type is unsupported in the current builder phase, return a clear error in the JSON result.

Output format:
{
  "blueprint_name": "BP_Example",
  "parent_class": "Actor",
  "blueprint_type": "Normal",
  "package_path": "/Game/LocalStudio/Blueprints",
  "components": [],
  "variables": [],
  "functions": [],
  "events": [],
  "nodes": [],
  "connections": []
}

Supported blueprint_type values:
- Normal
- Interface
- FunctionLibrary
- MacroLibrary
- Widget
- ActorComponent
- Character
- Pawn
- PlayerController
- GameMode
- GameState
- PlayerState
- AnimBlueprint

Return only raw valid JSON. No markdown fences.
)raw");
}

bool FLocalStudioBlueprintBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    return FLocalStudioJsonUtils::ExtractAndParseJsonObject(RawResponse, OutParsedData, OutError);
}

FString FLocalStudioBlueprintBuilder::PreparePromptContext(const FString& RawUserPrompt)
{
    FString Context = FString::Printf(
        TEXT("=== AVAILABLE BASE BLUEPRINT PARENT CLASSES ===\n")
        TEXT("Actor\nCharacter\nPawn\nPlayerController\nPlayerState\nGameModeBase\nGameStateBase\nActorComponent\nSceneComponent\nUserWidget\nGameInstance\n\n")
        TEXT("USER REQUEST: %s\n\n")
        TEXT("When creating a Blueprint, prefer a supported Unreal parent class and a standard Blueprint type.\n")
        TEXT("Do not invent unsupported Blueprint families or duplicate old logic.\n")
        TEXT("If the request is for a complete system, construct a Blueprint plan and leave graph node complexity disabled until the Blueprint asset pipeline is validated."),
        *RawUserPrompt
    );

    return Context;
}

UClass* FLocalStudioBlueprintBuilder::ResolveParentClass(const FString& InClassName)
{
    FString CleanClassName = InClassName.TrimStartAndEnd();
    if (CleanClassName.IsEmpty())
    {
        return AActor::StaticClass();
    }

    static const TMap<FString, UClass*> NativeClassMap = {
        { TEXT("Actor"), AActor::StaticClass() },
        { TEXT("Character"), ACharacter::StaticClass() },
        { TEXT("Pawn"), APawn::StaticClass() },
        { TEXT("PlayerController"), APlayerController::StaticClass() },
        { TEXT("PlayerState"), APlayerState::StaticClass() },
        { TEXT("GameModeBase"), AGameModeBase::StaticClass() },
        { TEXT("GameStateBase"), AGameStateBase::StaticClass() },
        { TEXT("ActorComponent"), UActorComponent::StaticClass() },
        { TEXT("SceneComponent"), USceneComponent::StaticClass() },
        { TEXT("UserWidget"), UUserWidget::StaticClass() },
        { TEXT("GameInstance"), UGameInstance::StaticClass() }
    };

    if (const UClass* const* Found = NativeClassMap.Find(CleanClassName))
    {
        return *Found;
    }

    if (UClass* ScriptClass = UClass::TryFindTypeSlow<UClass>(CleanClassName))
    {
        return ScriptClass;
    }

    if (UClass* CustomClass = FindFirstObject<UClass>(*CleanClassName))
    {
        return CustomClass;
    }

    if (!CleanClassName.StartsWith(TEXT("A")) && !CleanClassName.StartsWith(TEXT("U")))
    {
        if (UClass* PrefixedActorClass = FindFirstObject<UClass>(*(TEXT("A") + CleanClassName)))
        {
            return PrefixedActorClass;
        }
        if (UClass* PrefixedObjectClass = FindFirstObject<UClass>(*(TEXT("U") + CleanClassName)))
        {
            return PrefixedObjectClass;
        }
    }

    if (CleanClassName.Contains(TEXT("Character"), ESearchCase::IgnoreCase))
    {
        return ACharacter::StaticClass();
    }
    if (CleanClassName.Contains(TEXT("Controller"), ESearchCase::IgnoreCase))
    {
        return APlayerController::StaticClass();
    }
    if (CleanClassName.Contains(TEXT("Pawn"), ESearchCase::IgnoreCase))
    {
        return APawn::StaticClass();
    }
    if (CleanClassName.Contains(TEXT("Component"), ESearchCase::IgnoreCase))
    {
        return UActorComponent::StaticClass();
    }

    return AActor::StaticClass();
}

bool FLocalStudioBlueprintBuilder::ResolveEdGraphPinType(const FString& InTypeStr, const FString& InContainerStr, FEdGraphPinType& OutPinType)
{
    FString PinType = InTypeStr.TrimStartAndEnd().ToLower();
    FString ContainerType = InContainerStr.TrimStartAndEnd().ToLower();

    if (ContainerType == TEXT("array"))
    {
        OutPinType.ContainerType = EPinContainerType::Array;
    }
    else if (ContainerType == TEXT("set"))
    {
        OutPinType.ContainerType = EPinContainerType::Set;
    }
    else if (ContainerType == TEXT("map"))
    {
        OutPinType.ContainerType = EPinContainerType::Map;
    }
    else
    {
        OutPinType.ContainerType = EPinContainerType::None;
    }

    if (PinType == TEXT("bool") || PinType == TEXT("boolean"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        return true;
    }
    if (PinType == TEXT("byte") || PinType == TEXT("uint8"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
        return true;
    }
    if (PinType == TEXT("int") || PinType == TEXT("int32") || PinType == TEXT("integer"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        return true;
    }
    if (PinType == TEXT("float"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        return true;
    }
    if (PinType == TEXT("double"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
        return true;
    }
    if (PinType == TEXT("string"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
        return true;
    }
    if (PinType == TEXT("text"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
        return true;
    }
    if (PinType == TEXT("name"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        return true;
    }
    if (PinType == TEXT("vector"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        return true;
    }
    if (PinType == TEXT("vector2d") || PinType == TEXT("vector2d"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPinType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
        return true;
    }
    if (PinType == TEXT("rotator"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        return true;
    }
    if (PinType == TEXT("transform"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
        return true;
    }
    if (PinType == TEXT("object") || PinType == TEXT("actor"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        OutPinType.PinSubCategoryObject = AActor::StaticClass();
        return true;
    }
    return false;
}

bool FLocalStudioBlueprintBuilder::ProcessPromotableOperatorNode(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& NodeId, const FName& OperationName, const FName& MathLibraryFunctionName, const FString& FunctionName)
{
    if (!Blueprint || !FunctionGraph)
    {
        return false;
    }

    // Blueprint graph node creation is intentionally disabled in this phase.
    // This method remains as a compatibility stub and will be re-enabled later.
    UE_LOG(LogTemp, Log, TEXT("Blueprint graph node creation is temporarily disabled. Skipping node '%s' in function '%s'."), *NodeId, *FunctionName);
    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessComponents(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& ComponentsAdded, int32& ComponentsSkipped, int32& ComponentsAlreadyExisting)
{
    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ComponentsArray = nullptr;
    if (!ParsedData->TryGetArrayField(TEXT("components"), ComponentsArray))
    {
        return true;
    }

    for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentsArray)
    {
        if (!ComponentValue.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ComponentObject = ComponentValue->AsObject();
        if (!ComponentObject.IsValid())
        {
            continue;
        }

        FString ComponentName;
        if (!ComponentObject->TryGetStringField(TEXT("name"), ComponentName) || ComponentName.IsEmpty())
        {
            ComponentsSkipped++;
            continue;
        }

        FString ComponentType = TEXT("SceneComponent");
        ComponentObject->TryGetStringField(TEXT("type"), ComponentType);

        if (ComponentType.Equals(TEXT("SceneComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("StaticMeshComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("SkeletalMeshComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("BoxComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("SphereComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("CapsuleComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("CameraComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("SpringArmComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("AudioComponent"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("ChildActorComponent"), ESearchCase::IgnoreCase))
        {
            ComponentsAdded++;
            continue;
        }

        ComponentsSkipped++;
    }

    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessVariables(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& VariablesAdded, int32& VariablesSkipped, int32& VariablesAlreadyExisting)
{
    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
    if (!ParsedData->TryGetArrayField(TEXT("variables"), VariablesArray))
    {
        return true;
    }

    for (const TSharedPtr<FJsonValue>& VariableValue : *VariablesArray)
    {
        if (!VariableValue.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> VariableObject = VariableValue->AsObject();
        if (!VariableObject.IsValid())
        {
            continue;
        }

        FString VariableName;
        if (!VariableObject->TryGetStringField(TEXT("name"), VariableName) || VariableName.IsEmpty())
        {
            VariablesSkipped++;
            continue;
        }

        FString VariableType = TEXT("bool");
        VariableObject->TryGetStringField(TEXT("type"), VariableType);

        FEdGraphPinType TempPinType;
        if (!ResolveEdGraphPinType(VariableType, TEXT("single"), TempPinType))
        {
            VariablesSkipped++;
            continue;
        }

        if (Blueprint->NewVariables.ContainsByPredicate([&](const FBPVariableDescription& Var)
            {
                return Var.VarName == FName(*VariableName);
            }))
        {
            VariablesAlreadyExisting++;
            continue;
        }

        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), TempPinType);
        VariablesAdded++;
    }

    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessFunctions(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& FunctionsAdded, int32& FunctionsSkipped, int32& FunctionsAlreadyExisting)
{
    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* FunctionsArray = nullptr;
    if (!ParsedData->TryGetArrayField(TEXT("functions"), FunctionsArray))
    {
        return true;
    }

    for (const TSharedPtr<FJsonValue>& FunctionValue : *FunctionsArray)
    {
        if (!FunctionValue.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> FunctionObject = FunctionValue->AsObject();
        if (!FunctionObject.IsValid())
        {
            continue;
        }

        FString FunctionName;
        if (!FunctionObject->TryGetStringField(TEXT("function_name"), FunctionName) && !FunctionObject->TryGetStringField(TEXT("name"), FunctionName))
        {
            FunctionsSkipped++;
            continue;
        }

        if (FunctionName.IsEmpty())
        {
            FunctionsSkipped++;
            continue;
        }

        FName FunctionKey(*FunctionName);
        bool bAlreadyPresent = false;
        for (UEdGraph* ExistingGraph : Blueprint->FunctionGraphs)
        {
            if (ExistingGraph && ExistingGraph->GetFName() == FunctionKey)
            {
                bAlreadyPresent = true;
                break;
            }
        }

        if (bAlreadyPresent)
        {
            FunctionsAlreadyExisting++;
            continue;
        }

#if WITH_EDITOR
        UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionKey, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        if (!FunctionGraph)
        {
            FunctionsSkipped++;
            continue;
        }

        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, FunctionGraph, true, nullptr);
        UK2Node_FunctionEntry* EntryNode = nullptr;
        for (UEdGraphNode* Node : FunctionGraph->Nodes)
        {
            if (UK2Node_FunctionEntry* CastEntry = Cast<UK2Node_FunctionEntry>(Node))
            {
                EntryNode = CastEntry;
                break;
            }
        }

        if (!EntryNode)
        {
            EntryNode = NewObject<UK2Node_FunctionEntry>(FunctionGraph);
            EntryNode->SetFlags(RF_Transactional);
            FunctionGraph->AddNode(EntryNode);
            EntryNode->AllocateDefaultPins();
        }

        UK2Node_FunctionResult* ResultNode = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(EntryNode);
        if (ResultNode)
        {
            ResultNode->ReconstructNode();
        }

        if (EntryNode)
        {
            EntryNode->ReconstructNode();
        }

        FString ReturnType;
        if (FunctionObject->TryGetStringField(TEXT("return_type"), ReturnType) && !ReturnType.IsEmpty() && !ReturnType.Equals(TEXT("void"), ESearchCase::IgnoreCase))
        {
            FEdGraphPinType Type;
            if (ResolveEdGraphPinType(ReturnType, TEXT("single"), Type))
            {
                ResultNode->CreateUserDefinedPin(FName(TEXT("Result")), Type, EGPD_Input);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ParametersArray = nullptr;
        if (FunctionObject->TryGetArrayField(TEXT("parameters"), ParametersArray))
        {
            for (const TSharedPtr<FJsonValue>& ParameterValue : *ParametersArray)
            {
                if (!ParameterValue.IsValid())
                {
                    continue;
                }

                TSharedPtr<FJsonObject> ParameterObject = ParameterValue->AsObject();
                if (!ParameterObject.IsValid())
                {
                    continue;
                }

                FString Name, Type, Direction;
                ParameterObject->TryGetStringField(TEXT("name"), Name);
                ParameterObject->TryGetStringField(TEXT("type"), Type);
                ParameterObject->TryGetStringField(TEXT("direction"), Direction);

                if (Name.IsEmpty() || Type.IsEmpty())
                {
                    continue;
                }

                FEdGraphPinType PinType;
                if (!ResolveEdGraphPinType(Type, TEXT("single"), PinType))
                {
                    continue;
                }

                if (Direction.Equals(TEXT("Output"), ESearchCase::IgnoreCase) || Direction.Equals(TEXT("Return"), ESearchCase::IgnoreCase))
                {
                    if (ResultNode)
                    {
                        ResultNode->CreateUserDefinedPin(FName(*Name), PinType, EGPD_Input);
                    }
                }
                else
                {
                    EntryNode->CreateUserDefinedPin(FName(*Name), PinType, EGPD_Output);
                }
            }
        }

        if (EntryNode)
        {
            EntryNode->ReconstructNode();
        }
        if (ResultNode)
        {
            ResultNode->ReconstructNode();
        }
#endif

        FunctionsAdded++;
    }

    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessFunctionNodes(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, int32& NodesAdded, int32& NodesSkipped)
{
    NodesAdded = 0;
    NodesSkipped = 0;
    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
    if (!ParsedData->TryGetArrayField(TEXT("nodes"), NodesArray))
    {
        return true;
    }

    if (NodesArray && NodesArray->Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Blueprint graph node generation is temporarily disabled. Skipping %d node definitions."), NodesArray->Num());
        NodesSkipped += NodesArray->Num();
    }

    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessFunctionConnections(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, int32& ConnectionsMade, int32& ConnectionsSkipped)
{
    ConnectionsMade = 0;
    ConnectionsSkipped = 0;
    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray = nullptr;
    if (!ParsedData->TryGetArrayField(TEXT("connections"), ConnectionsArray))
    {
        return true;
    }

    if (ConnectionsArray && ConnectionsArray->Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Blueprint graph connection generation is temporarily disabled. Skipping %d connections."), ConnectionsArray->Num());
        ConnectionsSkipped += ConnectionsArray->Num();
    }

    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessMathNode(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const TSharedPtr<FJsonObject>& NodeObject, const FString& NodeId, const FString& NodeType, const FString& FunctionName)
{
    return true;
}

bool FLocalStudioBlueprintBuilder::CreateSquareRootNode(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& NodeId, const TSharedPtr<FJsonObject>& NodeObject, const FString& FunctionName)
{
    return true;
}

#if WITH_EDITOR
static bool IsSupportedBlueprintTypeForParentClass(EBlueprintType InType, UClass* ParentClass)
{
    if (!ParentClass)
    {
        return false;
    }

    if (ParentClass == UUserWidget::StaticClass() && InType == EBlueprintType::BPTYPE_Normal)
    {
        return true;
    }

    if (ParentClass == UActorComponent::StaticClass() || ParentClass == USceneComponent::StaticClass())
    {
        return InType == EBlueprintType::BPTYPE_Normal || InType == EBlueprintType::BPTYPE_FunctionLibrary;
    }

    if (ParentClass == APlayerController::StaticClass() || ParentClass == APlayerState::StaticClass() || ParentClass == AGameModeBase::StaticClass() || ParentClass == AGameStateBase::StaticClass())
    {
        return InType == EBlueprintType::BPTYPE_Normal;
    }

    return InType == EBlueprintType::BPTYPE_Normal || InType == EBlueprintType::BPTYPE_Interface || InType == EBlueprintType::BPTYPE_FunctionLibrary || InType == EBlueprintType::BPTYPE_MacroLibrary;
}
#endif
