// LocalStudioBlueprintBuilder.cpp
#include "Builder/LocalStudioBlueprintBuilder.h"
#include "Core/LocalStudioAssetUtils.h"
#include "Core/LocalStudioJsonUtils.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"

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
        const FString Normalized = RawType.TrimStartAndEnd().ToLower();

        if (Normalized.Contains(TEXT("interface")))
        {
            return BPTYPE_Interface;
        }
        if (Normalized.Contains(TEXT("macro")))
        {
            return BPTYPE_MacroLibrary;
        }
        if (Normalized.Contains(TEXT("function_library")) ||
            Normalized.Contains(TEXT("functionlibrary")) ||
            Normalized.Contains(TEXT("function library")))
        {
            return BPTYPE_FunctionLibrary;
        }
        if (Normalized.Contains(TEXT("levelscript")) ||
            Normalized.Contains(TEXT("level_script")))
        {
            return BPTYPE_LevelScript;
        }

        // Widget and animation Blueprint creation require specialized factories
        // and additional assets. Keep them on the normal creation path until
        // those factories are implemented rather than emitting invalid types.
        return BPTYPE_Normal;
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
    if (ParsedData->TryGetArrayField(TEXT("blueprints"), BlueprintsArray) &&
        BlueprintsArray &&
        BlueprintsArray->Num() > 0)
    {
        FString CombinedSummaries;
        bool bAllSucceeded = true;

        for (const TSharedPtr<FJsonValue>& BPValue : *BlueprintsArray)
        {
            if (!BPValue.IsValid() || BPValue->Type != EJson::Object)
            {
                bAllSucceeded = false;
                continue;
            }

            const FBuilderExecutionResult SingleResult = ExecutePlan(BPValue->AsObject());
            bAllSucceeded &= SingleResult.bSuccess;

            if (!SingleResult.UserSummary.IsEmpty())
            {
                CombinedSummaries += SingleResult.UserSummary + TEXT("\n\n");
            }
        }

        Result.bSuccess = bAllSucceeded;
        Result.UserSummary = CombinedSummaries.TrimEnd();
        return Result;
    }

    FString BlueprintName = ParsedData->HasField(TEXT("blueprint_name"))
        ? ParsedData->GetStringField(TEXT("blueprint_name"))
        : TEXT("BP_NewBlueprint");
    FString ParentClassName = ParsedData->HasField(TEXT("parent_class"))
        ? ParsedData->GetStringField(TEXT("parent_class"))
        : TEXT("Actor");
    const FString BlueprintTypeName = ParsedData->HasField(TEXT("blueprint_type"))
        ? ParsedData->GetStringField(TEXT("blueprint_type"))
        : TEXT("Normal");
    FString PackagePath = ParsedData->HasField(TEXT("package_path"))
        ? ParsedData->GetStringField(TEXT("package_path"))
        : TEXT("/Game/LocalStudio/Blueprints");

    if (ParentClassName.Equals(TEXT("Object"), ESearchCase::IgnoreCase) ||
        ParentClassName.Equals(TEXT("UObject"), ESearchCase::IgnoreCase))
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

    const EBlueprintType BlueprintType = ResolveBlueprintTypeFromString(BlueprintTypeName);

#if WITH_EDITOR
    UBlueprint* NewBlueprint = FLocalStudioAssetUtils::FindAssetByName<UBlueprint>(BlueprintName);
    const bool bCreatedBlueprint = NewBlueprint == nullptr;

    if (!NewBlueprint)
    {
        if (!PackagePath.StartsWith(TEXT("/Game/")))
        {
            PackagePath = TEXT("/Game/") + PackagePath;
        }
        if (!PackagePath.EndsWith(TEXT("/")))
        {
            PackagePath += TEXT("/");
        }

        const FString PackageString = PackagePath + BlueprintName;
        UPackage* Package = CreatePackage(*PackageString);
        if (!Package)
        {
            Result.UserSummary = FString::Printf(TEXT("Failed to create package for Blueprint '%s' at '%s'."), *BlueprintName, *PackageString);
            return Result;
        }

        NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
            ParentClass,
            Package,
            FName(*BlueprintName),
            BlueprintType,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass());

        if (!NewBlueprint)
        {
            Result.UserSummary = FString::Printf(TEXT("Failed to create Blueprint asset '%s' with parent '%s'."), *BlueprintName, *ParentClassName);
            return Result;
        }

        FAssetRegistryModule::AssetCreated(NewBlueprint);
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

    if (!ProcessComponents(NewBlueprint, ParsedData, ParentClass, ComponentsAdded, ComponentsSkipped, ComponentsAlreadyExisting) ||
        !ProcessVariables(NewBlueprint, ParsedData, ParentClass, VariablesAdded, VariablesSkipped, VariablesAlreadyExisting) ||
        !ProcessFunctions(NewBlueprint, ParsedData, ParentClass, FunctionsAdded, FunctionsSkipped, FunctionsAlreadyExisting))
    {
        Result.UserSummary = TEXT("Failed while processing Blueprint declarations.");
        return Result;
    }

    int32 NodesAdded = 0;
    int32 NodesSkipped = 0;
    ProcessFunctionNodes(NewBlueprint, ParsedData, NodesAdded, NodesSkipped);

    int32 ConnectionsMade = 0;
    int32 ConnectionsSkipped = 0;
    ProcessFunctionConnections(NewBlueprint, ParsedData, ConnectionsMade, ConnectionsSkipped);

    NewBlueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    const FString AssetPath = NewBlueprint->GetOutermost()->GetName();
    const FString PackageFilename = FPackageName::LongPackageNameToFilename(
        AssetPath,
        FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GWarn;
    SaveArgs.bForceByteSwapping = false;

    if (!UPackage::SavePackage(NewBlueprint->GetOutermost(), NewBlueprint, *PackageFilename, SaveArgs))
    {
        Result.UserSummary = FString::Printf(TEXT("Blueprint was created or updated but failed to save: %s"), *AssetPath);
        return Result;
    }

    Result.bSuccess = true;
    const FString BlueprintAction = bCreatedBlueprint ? TEXT("created") : TEXT("updated");
    Result.UserSummary = FString::Printf(
        TEXT("Successfully %s Blueprint [%s] at %s.\n")
        TEXT("Components: Added=%d Existing=%d Skipped=%d\n")
        TEXT("Variables: Added=%d Existing=%d Skipped=%d\n")
        TEXT("Functions: Added=%d Existing=%d Skipped=%d\n")
        TEXT("Blueprint Type: %s\n")
        TEXT("Parent Class: %s\n")
        TEXT("Graph nodes are currently deferred: %d skipped; connections deferred: %d."),
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
        *ParentClassName,
        NodesSkipped,
        ConnectionsSkipped);

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
Return one strict JSON object that LocalStudio translates into a Blueprint asset.

Supported blueprint_type values:
- Normal
- Interface
- FunctionLibrary
- MacroLibrary
- LevelScript
- EditorUtilityBlueprint
- Widget
- AnimBlueprint
- ActorComponent
- Character
- Pawn
- PlayerController
- GameMode
- GameState
- PlayerState

Rules:
- Implement only what the user explicitly requests.
- Use the requested parent_class exactly when it is available in the supplied project or Unreal context.
- Do not invent parent classes or silently replace a requested custom class.
- Components, variables, and function signatures may be generated.
- Graph nodes and graph connections are temporarily deferred during the Blueprint asset validation phase; leave nodes and connections empty unless specifically instructed otherwise.
- Return raw valid JSON only. Do not use markdown fences or commentary.

For function signatures, always use the "parameters" array.

Each parameter must contain:
{
  "name": "BaseDamage",
  "type": "Float",
  "direction": "Input"
}

Input parameters are added to FunctionEntry.
Output parameters are added to FunctionResult.
Do not use input_pins or output_pins unless compatibility with an older plan is required.

Schema:
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
)raw");
}

bool FLocalStudioBlueprintBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    return FLocalStudioJsonUtils::ExtractAndParseJsonObject(RawResponse, OutParsedData, OutError);
}

FString FLocalStudioBlueprintBuilder::PreparePromptContext(const FString& RawUserPrompt)
{
    FString CustomClassesList;
    const FString SourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
    TArray<FString> FoundHeaders;
    IFileManager::Get().FindFilesRecursive(FoundHeaders, *SourceDir, TEXT("*.h"), true, false, false);

    for (const FString& HeaderPath : FoundHeaders)
    {
        const FString BaseName = FPaths::GetBaseFilename(HeaderPath);
        if (!BaseName.EndsWith(TEXT(".generated")))
        {
            CustomClassesList += FString::Printf(TEXT("- %s\n"), *BaseName);
        }
    }

    return FString::Printf(
        TEXT("=== PROJECT CUSTOM C++ CLASSES ===\n%s\n")
        TEXT("=== STANDARD PARENT CLASSES ===\n")
        TEXT("Actor, Character, Pawn, PlayerController, PlayerState, GameModeBase, GameStateBase, ActorComponent, SceneComponent, UserWidget, GameInstance\n\n")
        TEXT("USER REQUEST:\n%s\n\n")
        TEXT("Use the exact requested custom C++ parent when it exists. Do not invent or silently substitute a parent class."),
        *CustomClassesList,
        *RawUserPrompt);
}

UClass* FLocalStudioBlueprintBuilder::ResolveParentClass(const FString& InClassName)
{
    const FString CleanClassName = InClassName.TrimStartAndEnd();
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

    if (UClass* const* Found = NativeClassMap.Find(CleanClassName))
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
    const FString PinType = InTypeStr.TrimStartAndEnd().ToLower();
    const FString ContainerType = InContainerStr.TrimStartAndEnd().ToLower();

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
    if (PinType == TEXT("vector2d"))
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

        const TSharedPtr<FJsonObject> ComponentObject = ComponentValue->AsObject();
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
        }
        else
        {
            ComponentsSkipped++;
        }
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

        const TSharedPtr<FJsonObject> VariableObject = VariableValue->AsObject();
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

        FEdGraphPinType PinType;
        if (!ResolveEdGraphPinType(VariableType, TEXT("single"), PinType))
        {
            VariablesSkipped++;
            continue;
        }

        bool bAlreadyExists = false;
        for (const FBPVariableDescription& ExistingVariable : Blueprint->NewVariables)
        {
            if (ExistingVariable.VarName == FName(*VariableName))
            {
                bAlreadyExists = true;
                break;
            }
        }

        if (bAlreadyExists)
        {
            VariablesAlreadyExisting++;
            continue;
        }

        if (FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType))
        {
            VariablesAdded++;
        }
        else
        {
            VariablesSkipped++;
        }
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

        const TSharedPtr<FJsonObject> FunctionObject = FunctionValue->AsObject();
        if (!FunctionObject.IsValid())
        {
            continue;
        }

        FString FunctionName;
        if (!FunctionObject->TryGetStringField(TEXT("function_name"), FunctionName) &&
            !FunctionObject->TryGetStringField(TEXT("name"), FunctionName))
        {
            FunctionsSkipped++;
            continue;
        }

        if (FunctionName.IsEmpty())
        {
            FunctionsSkipped++;
            continue;
        }

        const FName FunctionKey(*FunctionName);
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
        UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
            Blueprint,
            FunctionKey,
            UEdGraph::StaticClass(),
            UEdGraphSchema_K2::StaticClass());
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
        if (!EntryNode || !ResultNode)
        {
            FunctionsSkipped++;
            continue;
        }

        FString ReturnType;
        if (FunctionObject->TryGetStringField(TEXT("return_type"), ReturnType) &&
            !ReturnType.IsEmpty() &&
            !ReturnType.Equals(TEXT("void"), ESearchCase::IgnoreCase))
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

                const TSharedPtr<FJsonObject> ParameterObject = ParameterValue->AsObject();
                if (!ParameterObject.IsValid())
                {
                    continue;
                }

                FString Name;
                FString Type;
                FString Direction;
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

                if (Direction.Equals(TEXT("Output"), ESearchCase::IgnoreCase) ||
                    Direction.Equals(TEXT("Return"), ESearchCase::IgnoreCase))
                {
                    ResultNode->CreateUserDefinedPin(FName(*Name), PinType, EGPD_Input);
                }
                else
                {
                    EntryNode->CreateUserDefinedPin(FName(*Name), PinType, EGPD_Output);
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* InputPinsArray = nullptr;

if (FunctionObject->TryGetArrayField(TEXT("input_pins"), InputPinsArray))
{
    for (const TSharedPtr<FJsonValue>& PinValue : *InputPinsArray)
    {
        if (!PinValue.IsValid())
        {
            continue;
        }

        const TSharedPtr<FJsonObject> PinObject = PinValue->AsObject();
        if (!PinObject.IsValid())
        {
            continue;
        }

        FString PinName;
        FString PinType;

        if (!PinObject->TryGetStringField(TEXT("pin_name"), PinName))
        {
            PinObject->TryGetStringField(TEXT("name"), PinName);
        }

        if (!PinObject->TryGetStringField(TEXT("pin_type"), PinType))
        {
            PinObject->TryGetStringField(TEXT("type"), PinType);
        }

        if (PinName.IsEmpty() || PinType.IsEmpty())
        {
            continue;
        }

        FEdGraphPinType GraphPinType;
        if (!ResolveEdGraphPinType(PinType, TEXT("single"), GraphPinType))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Unsupported input pin type '%s' for '%s'."),
                *PinType,
                *PinName
            );
            continue;
        }

        EntryNode->CreateUserDefinedPin(
            FName(*PinName),
            GraphPinType,
            EGPD_Output
        );
    }
}

const TArray<TSharedPtr<FJsonValue>>* OutputPinsArray = nullptr;

if (FunctionObject->TryGetArrayField(TEXT("output_pins"), OutputPinsArray))
{
    for (const TSharedPtr<FJsonValue>& PinValue : *OutputPinsArray)
    {
        if (!PinValue.IsValid())
        {
            continue;
        }

        const TSharedPtr<FJsonObject> PinObject = PinValue->AsObject();
        if (!PinObject.IsValid())
        {
            continue;
        }

        FString PinName;
        FString PinType;

        if (!PinObject->TryGetStringField(TEXT("pin_name"), PinName))
        {
            PinObject->TryGetStringField(TEXT("name"), PinName);
        }

        if (!PinObject->TryGetStringField(TEXT("pin_type"), PinType))
        {
            PinObject->TryGetStringField(TEXT("type"), PinType);
        }

        if (PinName.IsEmpty() || PinType.IsEmpty())
        {
            continue;
        }

        FEdGraphPinType GraphPinType;
        if (!ResolveEdGraphPinType(PinType, TEXT("single"), GraphPinType))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Unsupported output pin type '%s' for '%s'."),
                *PinType,
                *PinName
            );
            continue;
        }

        ResultNode->CreateUserDefinedPin(
            FName(*PinName),
            GraphPinType,
            EGPD_Input
        );
    }
}

        EntryNode->ReconstructNode();
        ResultNode->ReconstructNode();

        
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
        NodesSkipped = NodesArray->Num();
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
        ConnectionsSkipped = ConnectionsArray->Num();
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