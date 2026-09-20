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
#include "UObject/UnrealType.h"
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

FBuilderExecutionResult FLocalStudioBlueprintBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = false;

    if (!ParsedData.IsValid())
    {
        Result.UserSummary = TEXT("Invalid Blueprint JSON payload.");
        return Result;
    }

    // ============================================================
    // 0. MULTI-BLUEPRINT HANDLING (Process array if present)
    // ============================================================
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

            // Recursive call for each single blueprint inside the array
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

    // ============================================================
    // 1. Parse Blueprint Name & Parent Class
    // ============================================================

    FString BPName = ParsedData->HasField(TEXT("blueprint_name"))
        ? ParsedData->GetStringField(TEXT("blueprint_name"))
        : TEXT("BP_NewBlueprint");

    FString ParentClassName = ParsedData->HasField(TEXT("parent_class"))
        ? ParsedData->GetStringField(TEXT("parent_class"))
        : TEXT("Actor");

    // Prevent UObject/Object from being passed into Blueprint creation
    if (ParentClassName.Equals(TEXT("Object"), ESearchCase::IgnoreCase) ||
        ParentClassName.Equals(TEXT("UObject"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning, TEXT("LocalStudio BlueprintBuilder: 'Object' specified as parent class. Falling back to 'Actor'."));
        ParentClassName = TEXT("Actor");
    }

    if (!BPName.StartsWith(TEXT("BP_")))
    {
        BPName = TEXT("BP_") + BPName;
    }

    // ============================================================
    // 2. Resolve Parent C++ Class
    // ============================================================

    UClass* ParentClass = nullptr;

    // 1. Try finding custom or native class directly
    ParentClass = UClass::TryFindTypeSlow<UClass>(ParentClassName);
    if (!ParentClass)
    {
        ParentClass = FindFirstObject<UClass>(*ParentClassName);
    }

    // 2. Fallback resolution for common hallucinations
    if (!ParentClass)
    {
        if (ParentClassName.Contains(TEXT("Character")))
        {
            ParentClass = ACharacter::StaticClass();
            UE_LOG(LogTemp, Warning, TEXT("Parent class '%s' not found. Falling back to ACharacter."), *ParentClassName);
        }
        else if (ParentClassName.Contains(TEXT("Pawn")))
        {
            ParentClass = APawn::StaticClass();
            UE_LOG(LogTemp, Warning, TEXT("Parent class '%s' not found. Falling back to APawn."), *ParentClassName);
        }
        else if (ParentClassName.Contains(TEXT("Controller")))
        {
            ParentClass = APlayerController::StaticClass();
            UE_LOG(LogTemp, Warning, TEXT("Parent class '%s' not found. Falling back to APlayerController."), *ParentClassName);
        }
        else
        {
            ParentClass = AActor::StaticClass();
            UE_LOG(LogTemp, Warning, TEXT("Parent class '%s' not found. Defaulting to AActor."), *ParentClassName);
        }
    }

    if (!ParentClass)
    {
        Result.UserSummary = FString::Printf(
            TEXT("Failed to find Parent C++ Class: %s"),
            *ParentClassName
        );

        return Result;
    }

    // ============================================================
    // 3. Validate Parent Class
    // ============================================================

    if (ParentClass->HasAnyClassFlags(
        CLASS_Abstract |
        CLASS_Deprecated |
        CLASS_NewerVersionExists))
    {
        Result.UserSummary = FString::Printf(
            TEXT("Parent class '%s' is abstract, deprecated, or invalid for Blueprint creation."),
            *ParentClassName
        );

        return Result;
    }

    // ============================================================
    // 4. FIND EXISTING BLUEPRINT ANYWHERE IN PROJECT
    // ============================================================

    UBlueprint* NewBP =
        FLocalStudioAssetUtils::FindAssetByName<UBlueprint>(BPName);

    UPackage* Package = nullptr;
    bool bCreatedBlueprint = false;

    if (NewBP)
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT("LocalStudio BlueprintBuilder: Found existing Blueprint '%s' at '%s'. Reusing it."),
            *BPName,
            *NewBP->GetPathName()
        );

        Package = NewBP->GetOutermost();
    }
    else
    {
        // ========================================================
        // 5. Blueprint doesn't exist anywhere.
        // Create it in the default LocalStudio location.
        // ========================================================

        FString PackagePath =
            TEXT("/Game/LocalStudio/Blueprints/") + BPName;

        Package = CreatePackage(*PackagePath);

        if (!Package)
        {
            Result.UserSummary = FString::Printf(
                TEXT("Failed to create Blueprint package: %s"),
                *PackagePath
            );

            return Result;
        }

        NewBP = FKismetEditorUtilities::CreateBlueprint(
            ParentClass,
            Package,
            FName(*BPName),
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass()
        );

        if (!NewBP)
        {
            Result.UserSummary =
                TEXT("Failed to instantiate Blueprint asset.");

            return Result;
        }

        bCreatedBlueprint = true;

        FAssetRegistryModule::AssetCreated(NewBP);

        UE_LOG(
            LogTemp,
            Log,
            TEXT("LocalStudio BlueprintBuilder: Created new Blueprint '%s' at '%s'."),
            *BPName,
            *NewBP->GetPathName()
        );
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

    if (!ProcessComponents(
        NewBP,
        ParsedData,
        ParentClass,
        ComponentsAdded,
        ComponentsSkipped,
        ComponentsAlreadyExisting))
    {
        Result.UserSummary =
            TEXT("Failed while processing Blueprint components.");

        return Result;
    }

    if (!ProcessVariables(
        NewBP,
        ParsedData,
        ParentClass,
        VariablesAdded,
        VariablesSkipped,
        VariablesAlreadyExisting))
    {
        Result.UserSummary =
            TEXT("Failed while processing Blueprint variables.");

        return Result;
    }

    if (!ProcessFunctions(
        NewBP,
        ParsedData,
        ParentClass,
        FunctionsAdded,
        FunctionsSkipped,
        FunctionsAlreadyExisting))
    {
        Result.UserSummary =
            TEXT("Failed while processing Blueprint functions.");

        return Result;
    }

    // ============================================================
    // 6A. PROCESS FUNCTION GRAPH NODES
    // ============================================================

    int32 NodesAdded = 0;
    int32 NodesSkipped = 0;

    if (!ProcessFunctionNodes(
        NewBP,
        ParsedData,
        NodesAdded,
        NodesSkipped))
    {
        Result.UserSummary =
            TEXT("Failed while processing Blueprint graph nodes.");

        return Result;
    }

    // ============================================================
    // 6B. PROCESS FUNCTION GRAPH CONNECTIONS
    // ============================================================

    int32 ConnectionsMade = 0;
    int32 ConnectionsSkipped = 0;

    if (!ProcessFunctionConnections(
        NewBP,
        ParsedData,
        ConnectionsMade,
        ConnectionsSkipped))
    {
        Result.UserSummary =
            TEXT("Failed while processing Blueprint graph connections.");

        return Result;
    }

    // ============================================================
    // 7. MARK BLUEPRINT DIRTY
    // ============================================================

    NewBP->MarkPackageDirty();

    // ============================================================
    // 8. COMPILE
    // ============================================================

    FKismetEditorUtilities::CompileBlueprint(NewBP);

    // ============================================================
    // 9. SAVE BLUEPRINT
    // ============================================================

    FString ActualPackagePath =
        NewBP->GetOutermost()->GetName();

    FString PackageFileName =
        FPackageName::LongPackageNameToFilename(
            ActualPackagePath,
            FPackageName::GetAssetPackageExtension()
        );

    FSavePackageArgs SaveArgs;

    SaveArgs.TopLevelFlags =
        RF_Public | RF_Standalone;

    SaveArgs.Error = GWarn;
    SaveArgs.bForceByteSwapping = false;

    if (!UPackage::SavePackage(
        NewBP->GetOutermost(),
        NewBP,
        *PackageFileName,
        SaveArgs))
    {
        Result.UserSummary = FString::Printf(
            TEXT("Blueprint was created/updated but failed to save: %s"),
            *ActualPackagePath
        );

        return Result;
    }

    // ============================================================
    // 10. RESULT
    // ============================================================

    Result.bSuccess = true;

    FString BlueprintAction = bCreatedBlueprint ? TEXT("created") : TEXT("updated");

    Result.UserSummary = FString::Printf(
        TEXT(
            "Successfully %s Blueprint [%s] at %s.\n"
            "� Components - Added: %d | Existing: %d | Skipped: %d\n"
            "� Variables  - Added: %d | Existing: %d | Skipped: %d\n"
            "� Functions  - Added: %d | Existing: %d | Skipped: %d\n"
            "� Graph Nodes - Added: %d | Skipped: %d\n"
            "� Connections - Made: %d | Skipped: %d"
        ),
        *BlueprintAction,
        *BPName,
        *ActualPackagePath,

        ComponentsAdded,
        ComponentsAlreadyExisting,
        ComponentsSkipped,

        VariablesAdded,
        VariablesAlreadyExisting,
        VariablesSkipped,

        FunctionsAdded,
        FunctionsAlreadyExisting,
        FunctionsSkipped,

        NodesAdded,
        NodesSkipped,

        ConnectionsMade,
        ConnectionsSkipped
    );

    return Result;
}

bool FLocalStudioBlueprintBuilder::ProcessVariables(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& VariablesAdded, int32& VariablesSkipped, int32& VariablesAlreadyExisting)
{
    const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
    if (!ParsedData->TryGetArrayField(TEXT("variables"), VariablesArray))
    {
        return true;
    }

    for (const TSharedPtr<FJsonValue>& VariableValue : *VariablesArray)
    {
        if (!VariableValue.IsValid()) continue;
        TSharedPtr<FJsonObject> VariableObject = VariableValue->AsObject();
        if (!VariableObject.IsValid()) continue;

        FString VariableName;
        if (!VariableObject->TryGetStringField(TEXT("name"), VariableName) || VariableName.IsEmpty())
        {
            continue;
        }

        FString VariableType = TEXT("bool");
        VariableObject->TryGetStringField(TEXT("type"), VariableType);

        FString ContainerType = TEXT("single");
        if (!VariableObject->TryGetStringField(TEXT("container_type"), ContainerType))
        {
            VariableObject->TryGetStringField(TEXT("container"), ContainerType);
        }

        FName VariableFName(*VariableName);

        // Check conflicts...
        bool bVariableExists = false;
        for (const FBPVariableDescription& ExistingVariable : Blueprint->NewVariables)
        {
            if (ExistingVariable.VarName == VariableFName)
            {
                bVariableExists = true;
                break;
            }
        }

        if (bVariableExists)
        {
            VariablesAlreadyExisting++;
            continue;
        }

        FEdGraphPinType PinType;
        if (!ResolveEdGraphPinType(VariableType, ContainerType, PinType))
        {
            VariablesSkipped++;
            continue;
        }

        if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableFName, PinType))
        {
            VariablesSkipped++;
            continue;
        }

        VariablesAdded++;
    }

    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessMathNode(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const TSharedPtr<FJsonObject>& NodeObject, const FString& NodeId, const FString& NodeType, const FString& FunctionName)
{
    if (!Blueprint || !FunctionGraph || !NodeObject.IsValid())
    {
        return false;
    }

    // Handle square root specifically
    if (NodeType.Equals(TEXT("Math_SquareRoot"), ESearchCase::IgnoreCase) || 
        NodeType.Contains(TEXT("sqrt"), ESearchCase::IgnoreCase))
    {
        return CreateSquareRootNode(Blueprint, FunctionGraph, NodeId, NodeObject, FunctionName);
    }
    
    // Handle other math operations - existing logic
    FName MathFunc = NAME_None;
    FString NodeTitle = TEXT("Math Operation");
    
    if (NodeType.Equals(TEXT("Math_Add"), ESearchCase::IgnoreCase) || 
        NodeType.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
    {
        MathFunc = FName(TEXT("Add_DoubleDouble"));
        NodeTitle = TEXT("Add");
    }
    else if (NodeType.Equals(TEXT("Math_Subtract"), ESearchCase::IgnoreCase) || 
             NodeType.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase))
    {
        MathFunc = FName(TEXT("Subtract_DoubleDouble"));
        NodeTitle = TEXT("Subtract");
    }
    else if (NodeType.Equals(TEXT("Math_Multiply"), ESearchCase::IgnoreCase) || 
             NodeType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
    {
        MathFunc = FName(TEXT("Multiply_DoubleDouble"));
        NodeTitle = TEXT("Multiply");
    }
    else if (NodeType.Equals(TEXT("Math_Divide"), ESearchCase::IgnoreCase) || 
             NodeType.Equals(TEXT("Divide"), ESearchCase::IgnoreCase))
    {
        MathFunc = FName(TEXT("Divide_DoubleDouble"));
        NodeTitle = TEXT("Divide");
    }
    else if (NodeType.Equals(TEXT("Math_Modulo"), ESearchCase::IgnoreCase) || 
             NodeType.Equals(TEXT("Modulo"), ESearchCase::IgnoreCase))
    {
        MathFunc = FName(TEXT("GenericPercent_FloatFloat"));
        NodeTitle = TEXT("Modulo");
    }
    else if (NodeType.Equals(TEXT("Math_Abs"), ESearchCase::IgnoreCase) || 
             NodeType.Equals(TEXT("Abs"), ESearchCase::IgnoreCase))
    {
        MathFunc = FName(TEXT("Abs_Double"));
        NodeTitle = TEXT("Absolute Value");
    }

    // If we found a valid math function, create it
    if (MathFunc != NAME_None)
    {
        return ProcessPromotableOperatorNode(Blueprint, FunctionGraph, NodeId, MathFunc, MathFunc, FunctionName);
    }
    
    return false;
}

bool FLocalStudioBlueprintBuilder::ResolveEdGraphPinType(const FString& InTypeStr, const FString& InContainerStr, FEdGraphPinType& OutPinType)
{
    FString PinType = InTypeStr.ToLower().TrimStartAndEnd();
    FString ContainerType = InContainerStr.ToLower().TrimStartAndEnd();

    // 1. Resolve Container Type
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

    // 2. Resolve Pin Data Type
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
    if (PinType == TEXT("int64") || PinType == TEXT("integer64"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
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
    if (!Blueprint || !FunctionGraph)
    {
        return false;
    }

    // Check if node already exists
    for (UEdGraphNode* ExistingNode : FunctionGraph->Nodes)
    {
        if (UK2Node_PromotableOperator* ExistingOperator = Cast<UK2Node_PromotableOperator>(ExistingNode))
        {
            if (ExistingOperator->GetName().Equals(NodeId, ESearchCase::IgnoreCase) ||
                (ExistingOperator->GetOperationName() == MathLibraryFunctionName))
            {
                return true;
            }
        }
    }

    // Create new math node
    UK2Node_PromotableOperator* OpNode = NewObject<UK2Node_PromotableOperator>(FunctionGraph, NAME_None, RF_Transactional);
    if (!OpNode)
    {
        return false;
    }

    FunctionGraph->AddNode(OpNode, true, false);
    OpNode->Rename(*NodeId, FunctionGraph, REN_DontCreateRedirectors | REN_NonTransactional);
    OpNode->CreateNewGuid();
    OpNode->FunctionReference = FMemberReference();
    OpNode->FunctionReference.SetExternalMember(MathLibraryFunctionName, UKismetMathLibrary::StaticClass());
    OpNode->bDefaultsToPureFunc = true;
    
    // Note: NodeTitle property doesn't exist on UK2Node_PromotableOperator
    // We can set the node's display name through other means if needed
    
    OpNode->PostPlacedNewNode();
    OpNode->AllocateDefaultPins();
    OpNode->ReconstructNode();

    FunctionGraph->Modify();
    OpNode->Modify();

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    return true;
}
#endif

FString FLocalStudioBlueprintBuilder::GetTechnicalGuardrails() const
{
    FString Prompt;

    Prompt += TEXT(R"raw(
You are LocalStudio, an Unreal Engine 5.7 Blueprint development assistant.
Your job is to produce a strict JSON intermediate representation that LocalStudio translates into an Unreal Engine Blueprint.
The C++ builder is authoritative for asset creation, graph construction, node/pin implementation, compilation, and saving.

=== USER REQUEST VALIDATION & MINIMALITY ===
- Implement ONLY what the user explicitly requested (functions, nodes, connections, variables, components).
- If not explicitly requested, arrays ("functions", "nodes", "connections", "variables", "components") MUST be empty.
- Do not invent speculative assets, variables, helper logic, C++ classes, APIs, or graph behavior.
- Prefer the smallest graph that correctly implements the request.

=== BLUEPRINT GENERATION & MULTI-BLUEPRINT RULES ===
- Create one object in the "blueprints" array per requested asset (matching the requested count).
- Each object requires independent fields: blueprint_name, parent_class, components, variables, functions, events, nodes, connections.
- Apply "BP_" prefix independently. Never fallback to or merge into "BP_Actor".
- Context modification applies ONLY when explicitly editing the currently selected Blueprint.
- Executing plans must be idempotent; do not create duplicate assets or graph elements. Search existing project paths before creating new ones.

=== PARENT CLASS RESOLUTION ===
1. Custom Class Match: Exact match from PROJECT CUSTOM C++ CLASSES list if explicitly requested/implied.
2. Engine Fallback: Use standard types: "Character", "Pawn", "Actor", "PlayerController", "GameModeBase", "ActorComponent".
3. Prohibitions: NEVER invent parent class names by stripping "BP_" prefixes.

=== C++ INTEROP & REPLICATION ===
- Inspect parent class context. Do NOT recreate or duplicate existing C++ properties, functions, or replication systems in Blueprint.
- Use inherited, Blueprint-visible C++ members directly.

=== FUNCTIONS & PARAMETERS ===
Schema:
{
  "function_name": "Calculate",
  "parameters": [
    { "name": "A", "type": "Float", "direction": "Input" },
    { "name": "Result", "type": "Float", "direction": "Output" }
  ],
  "return_type": "Float" // Permitted ONLY for single unnamed returns. Omit if Output parameters exist.
}
- "Input" parameters become pins on FunctionEntry; "Output" parameters become pins on FunctionResult.
- Represent multiple outputs as separate "Output" direction parameters. Do NOT collapse them.

=== GRAPH NODES & IMPLICIT REFERENCES ===
- "nodes" array contains ONLY executable graph nodes. NEVER create nodes for FunctionEntry, FunctionResult, or variables/parameters.
- Node Schema:
  {
    "node_id": "Add_A_B",
    "node_type": "Math_Add",
    "function_name": "Calculate",
    "position": { "x": 50, "y": 50 } // Include "position" ONLY if explicitly requested by user.
  }
- Implicit Node Connectors (Connection references only, NOT graph nodes):
  - Input Entry Pin Source: "from_node": "Function_<function_name>"
  - Output Result Pin Target: "to_node": "FunctionResult_<function_name>"

=== CONNECTIONS ===
Schema:
{
  "function_name": "Calculate",
  "from_node": "Function_Calculate",
  "from_pin": "A",
  "to_node": "Add_A_B",
  "to_pin": "A"
}
- Every connection MUST connect compatible pins. Node IDs must be unique within the graph.

=== MATH & FLOAT RULES ===
Supported Node Types: Math_Add, Math_Subtract, Math_Multiply, Math_Divide, Math_Modulo.
- Inputs: A, B | Output: ReturnValue
- Operands maintain order: Math_Subtract (A - B), Math_Divide (A / B), Math_Modulo (A % B).
- Standard representation: Always use "Float" for Blueprint float types. Do not emit "Double" or construct float-to-double conversions unless explicitly requested.

=== OUTPUT FORMAT ===
Return ONLY raw valid JSON. No markdown code fences, no leading/trailing text or commentary.

Single Blueprint Output Schema:
{
  "blueprint_name": "BP_Calculator",
  "parent_class": "Actor",
  "components": [],
  "variables": [],
  "functions": [],
  "events": [],
  "nodes": [],
  "connections": []
}
)raw");

    return Prompt;
}

bool FLocalStudioBlueprintBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    // Delegate to the central JSON Utility helper to strip backticks, control characters, and extract JSON
    return FLocalStudioJsonUtils::ExtractAndParseJsonObject(RawResponse, OutParsedData, OutError);
}

FString FLocalStudioBlueprintBuilder::PreparePromptContext(const FString& RawUserPrompt)
{
    FString CustomClassesList;
    FString SourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
    TArray<FString> FoundHeaders;

    IFileManager::Get().FindFilesRecursive(FoundHeaders, *SourceDir, TEXT("*.h"), true, false, false);

    for (const FString& HeaderPath : FoundHeaders)
    {
        FString BaseName = FPaths::GetBaseFilename(HeaderPath);

        // Skip generated headers or component classes to avoid confusing parent class resolution
        if (BaseName.EndsWith(TEXT(".generated")) || BaseName.Contains(TEXT("Component")))
        {
            continue;
        }

        CustomClassesList += FString::Printf(TEXT("- %s\n"), *BaseName);
    }

    FString Context = FString::Printf(
        TEXT("=== PROJECT CUSTOM C++ CLASSES ===\n")
        TEXT("%s\n")
        TEXT("CRITICAL INSTRUCTION:\n")
        TEXT("1. For standard Blueprint types, use ONLY standard engine class names: 'Actor', 'Character', 'Pawn', 'PlayerController', 'GameModeBase', 'ActorComponent'.\n")
        TEXT("2. Do NOT use custom project C++ classes as parent_class UNLESS the user explicitly requests to inherit from a specific custom class.\n")
        TEXT("3. Do NOT append 'Player' or other modifiers to parent class names (e.g. use 'Character', NEVER 'PlayerCharacter').\n\n")
        TEXT("USER REQUEST: %s"),
        *CustomClassesList,
        *RawUserPrompt
    );

    return Context;
}

UClass* FLocalStudioBlueprintBuilder::ResolveParentClass(const FString& InClassName)
{
    FString CleanClassName = InClassName.TrimStartAndEnd();

    // 1. Direct Engine Native Type Lookup Map
    static const TMap<FString, UClass*> NativeClassMap = {
        { TEXT("Actor"),            AActor::StaticClass() },
        { TEXT("Character"),        ACharacter::StaticClass() },
        { TEXT("Pawn"),             APawn::StaticClass() },
        { TEXT("PlayerController"), APlayerController::StaticClass() },
        { TEXT("GameModeBase"),     AGameModeBase::StaticClass() },
        { TEXT("ActorComponent"),   UActorComponent::StaticClass() }
    };

    if (UClass* const* FoundClass = NativeClassMap.Find(CleanClassName))
    {
        return *FoundClass;
    }

    // 2. Exact Native Script Path Lookup
    FString ScriptPath = FString::Printf(TEXT("/Script/Engine.%s"), *CleanClassName);
    if (UClass* ScriptClass = UClass::TryFindTypeSlow<UClass>(ScriptPath))
    {
        return ScriptClass;
    }

    // 3. Custom Project Class Lookup via FindFirstObject
    if (UClass* CustomClass = FindFirstObject<UClass>(*CleanClassName))
    {
        return CustomClass;
    }

    // 4. Try resolving with standard Unreal C++ prefixes (A, U)
    if (!CleanClassName.StartsWith(TEXT("A")) && !CleanClassName.StartsWith(TEXT("U")))
    {
        if (UClass* PrefixedActorClass = FindFirstObject<UClass>(*(TEXT("A") + CleanClassName)))
        {
            return PrefixedActorClass;
        }
        if (UClass* PrefixedObjClass = FindFirstObject<UClass>(*(TEXT("U") + CleanClassName)))
        {
            return PrefixedObjClass;
        }
    }

    // 5. Hardened Fuzzy Fallback for Common LLM Hallucinations
    if (CleanClassName.Contains(TEXT("Character"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning, TEXT("LocalStudio: Could not resolve parent class '%s'. Falling back to ACharacter."), *InClassName);
        return ACharacter::StaticClass();
    }
    if (CleanClassName.Contains(TEXT("Controller"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning, TEXT("LocalStudio: Could not resolve parent class '%s'. Falling back to APlayerController."), *InClassName);
        return APlayerController::StaticClass();
    }
    if (CleanClassName.Contains(TEXT("Pawn"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning, TEXT("LocalStudio: Could not resolve parent class '%s'. Falling back to APawn."), *InClassName);
        return APawn::StaticClass();
    }
    if (CleanClassName.Contains(TEXT("Component"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning, TEXT("LocalStudio: Could not resolve parent class '%s'. Falling back to UActorComponent."), *InClassName);
        return UActorComponent::StaticClass();
    }

    // Default Fallback
    UE_LOG(LogTemp, Warning, TEXT("LocalStudio: Could not resolve parent class '%s'. Defaulting to AActor."), *InClassName);
    return AActor::StaticClass();
}

bool FLocalStudioBlueprintBuilder::ProcessComponents(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& ComponentsAdded, int32& ComponentsSkipped, int32& ComponentsAlreadyExisting)
{
    // ============================================================
    // PROCESS BLUEPRINT COMPONENTS
    // ============================================================

    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ComponentsArray = nullptr;

    if (!ParsedData->TryGetArrayField(
        TEXT("components"),
        ComponentsArray))
    {
        // No components requested is a valid result.
        return true;
    }

    // ============================================================
    // PASS 1:
    // CREATE ALL COMPONENTS
    //
    // We intentionally create every component before resolving
    // attachments. This allows the LLM to list a child component
    // before its parent without breaking the hierarchy.
    // ============================================================

    TMap<FName, USCS_Node*> ComponentNodes;

    for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentsArray)
    {
        if (!ComponentValue.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ComponentObject =
            ComponentValue->AsObject();

        if (!ComponentObject.IsValid())
        {
            continue;
        }

        // ========================================================
        // COMPONENT NAME
        // ========================================================

        FString ComponentName;

        if (!ComponentObject->TryGetStringField(
            TEXT("name"),
            ComponentName))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Component entry is missing 'name'.")
            );

            ComponentsSkipped++;
            continue;
        }

        ComponentName.TrimStartAndEndInline();

        if (ComponentName.IsEmpty())
        {
            ComponentsSkipped++;
            continue;
        }

        const FName ComponentFName(*ComponentName);

        // ========================================================
        // COMPONENT TYPE
        // ========================================================

        FString ComponentType = TEXT("SceneComponent");

        ComponentObject->TryGetStringField(
            TEXT("type"),
            ComponentType
        );

        ComponentType.TrimStartAndEndInline();

        // ========================================================
        // CHECK FOR EXISTING COMPONENT
        // ========================================================

        USCS_Node* ExistingNode = nullptr;

        if (Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node :
                Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node &&
                    Node->GetVariableName() == ComponentFName)
                {
                    ExistingNode = Node;
                    break;
                }
            }
        }

        if (ExistingNode)
        {
            ComponentsAlreadyExisting++;

            // Keep the existing component available to the attachment
            // pass. This is important when a new component is being
            // attached to a component that already exists.
            ComponentNodes.Add(ComponentFName, ExistingNode);

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio BlueprintBuilder: Component '%s' already exists in Blueprint '%s'. Not creating duplicate."),
                *ComponentName,
                *Blueprint->GetPathName()
            );

            continue;
        }

        // ========================================================
        // RESOLVE COMPONENT CLASS
        // ========================================================

        UClass* ComponentClass = nullptr;

        if (ComponentType.Equals(
            TEXT("SceneComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = USceneComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("StaticMeshComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = UStaticMeshComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("SkeletalMeshComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = USkeletalMeshComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("BoxComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = UBoxComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("SphereComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = USphereComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("CapsuleComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = UCapsuleComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("ArrowComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = UArrowComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("AudioComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = UAudioComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("CameraComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = UCameraComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("SpringArmComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = USpringArmComponent::StaticClass();
        }
        else if (ComponentType.Equals(
            TEXT("ChildActorComponent"),
            ESearchCase::IgnoreCase))
        {
            ComponentClass = UChildActorComponent::StaticClass();
        }
        else
        {
            // Try resolving a component class dynamically.
            FString ClassName = ComponentType;

            if (!ClassName.EndsWith(TEXT("Component")))
            {
                ClassName += TEXT("Component");
            }

            ComponentClass = FindFirstObject<UClass>(
                *ClassName,
                EFindFirstObjectOptions::None
            );

            if (!ComponentClass)
            {
                ComponentClass = FindFirstObject<UClass>(
                    *(TEXT("U") + ClassName),
                    EFindFirstObjectOptions::None
                );
            }
        }

        // ========================================================
        // VALIDATE COMPONENT CLASS
        // ========================================================

        if (!ComponentClass ||
            !ComponentClass->IsChildOf(
                USceneComponent::StaticClass()))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Unsupported component type '%s' for component '%s'."),
                *ComponentType,
                *ComponentName
            );

            ComponentsSkipped++;
            continue;
        }

        // ========================================================
        // CREATE SCS COMPONENT
        // ========================================================

        if (!Blueprint->SimpleConstructionScript)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Blueprint '%s' has no SimpleConstructionScript."),
                *Blueprint->GetPathName()
            );

            ComponentsSkipped++;
            continue;
        }

        USCS_Node* NewNode =
            Blueprint->SimpleConstructionScript->CreateNode(
                ComponentClass,
                ComponentFName
            );

        if (!NewNode)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Failed to create component '%s' of type '%s'."),
                *ComponentName,
                *ComponentType
            );

            ComponentsSkipped++;
            continue;
        }

        // ========================================================
        // ADD TO ROOT TEMPORARILY
        //
        // Every component is initially registered with the SCS.
        // The attachment pass below will move children under their
        // requested parents.
        // ========================================================

        Blueprint->SimpleConstructionScript->AddNode(NewNode);

        // ========================================================
        // COMPONENT TRANSFORM
        //
        // Defaults:
        // Location = 0,0,0
        // Rotation = 0,0,0
        // Scale    = 1,1,1
        //
        // The JSON can override any of these.
        // ========================================================

        if (USceneComponent* SceneComponent =
            Cast<USceneComponent>(NewNode->ComponentTemplate))
        {
            FVector RelativeLocation = FVector::ZeroVector;
            FRotator RelativeRotation = FRotator::ZeroRotator;
            FVector RelativeScale = FVector::OneVector;

            // ----------------------------------------------------
            // LOCATION
            // ----------------------------------------------------

            const TArray<TSharedPtr<FJsonValue>>* LocationArray = nullptr;

            if (ComponentObject->TryGetArrayField(
                TEXT("location"),
                LocationArray) &&
                LocationArray->Num() >= 3)
            {
                RelativeLocation = FVector(
                    static_cast<float>((*LocationArray)[0]->AsNumber()),
                    static_cast<float>((*LocationArray)[1]->AsNumber()),
                    static_cast<float>((*LocationArray)[2]->AsNumber())
                );
            }
            else if (ComponentObject->TryGetArrayField(
                TEXT("relative_location"),
                LocationArray) &&
                LocationArray->Num() >= 3)
            {
                RelativeLocation = FVector(
                    static_cast<float>((*LocationArray)[0]->AsNumber()),
                    static_cast<float>((*LocationArray)[1]->AsNumber()),
                    static_cast<float>((*LocationArray)[2]->AsNumber())
                );
            }

            // ----------------------------------------------------
            // ROTATION
            // ----------------------------------------------------

            const TArray<TSharedPtr<FJsonValue>>* RotationArray = nullptr;

            if (ComponentObject->TryGetArrayField(
                TEXT("rotation"),
                RotationArray) &&
                RotationArray->Num() >= 3)
            {
                RelativeRotation = FRotator(
                    static_cast<float>((*RotationArray)[0]->AsNumber()),
                    static_cast<float>((*RotationArray)[1]->AsNumber()),
                    static_cast<float>((*RotationArray)[2]->AsNumber())
                );
            }
            else if (ComponentObject->TryGetArrayField(
                TEXT("relative_rotation"),
                RotationArray) &&
                RotationArray->Num() >= 3)
            {
                RelativeRotation = FRotator(
                    static_cast<float>((*RotationArray)[0]->AsNumber()),
                    static_cast<float>((*RotationArray)[1]->AsNumber()),
                    static_cast<float>((*RotationArray)[2]->AsNumber())
                );
            }

            // ----------------------------------------------------
            // SCALE
            // ----------------------------------------------------

            const TArray<TSharedPtr<FJsonValue>>* ScaleArray = nullptr;

            if (ComponentObject->TryGetArrayField(
                TEXT("scale"),
                ScaleArray) &&
                ScaleArray->Num() >= 3)
            {
                RelativeScale = FVector(
                    static_cast<float>((*ScaleArray)[0]->AsNumber()),
                    static_cast<float>((*ScaleArray)[1]->AsNumber()),
                    static_cast<float>((*ScaleArray)[2]->AsNumber())
                );
            }
            else if (ComponentObject->TryGetArrayField(
                TEXT("relative_scale"),
                ScaleArray) &&
                ScaleArray->Num() >= 3)
            {
                RelativeScale = FVector(
                    static_cast<float>((*ScaleArray)[0]->AsNumber()),
                    static_cast<float>((*ScaleArray)[1]->AsNumber()),
                    static_cast<float>((*ScaleArray)[2]->AsNumber())
                );
            }

            SceneComponent->SetRelativeLocation(RelativeLocation);
            SceneComponent->SetRelativeRotation(RelativeRotation);
            SceneComponent->SetRelativeScale3D(RelativeScale);
        }

        // ========================================================
        // STORE NODE FOR ATTACHMENT PASS
        // ========================================================

        ComponentNodes.Add(ComponentFName, NewNode);

        ComponentsAdded++;

        UE_LOG(
            LogTemp,
            Log,
            TEXT("LocalStudio BlueprintBuilder: Created component '%s' of type '%s' in Blueprint '%s'."),
            *ComponentName,
            *ComponentType,
            *Blueprint->GetPathName()
        );
    }

    // ============================================================
    // PASS 2:
    // RESOLVE COMPONENT ATTACHMENTS
    // ============================================================

    for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentsArray)
    {
        if (!ComponentValue.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ComponentObject =
            ComponentValue->AsObject();

        if (!ComponentObject.IsValid())
        {
            continue;
        }

        FString ComponentName;

        if (!ComponentObject->TryGetStringField(
            TEXT("name"),
            ComponentName))
        {
            continue;
        }

        ComponentName.TrimStartAndEndInline();

        if (ComponentName.IsEmpty())
        {
            continue;
        }

        // --------------------------------------------------------
        // GET COMPONENT NODE
        // --------------------------------------------------------

        USCS_Node** ChildNodePtr =
            ComponentNodes.Find(FName(*ComponentName));

        if (!ChildNodePtr || !*ChildNodePtr)
        {
            continue;
        }

        USCS_Node* ChildNode = *ChildNodePtr;

        // --------------------------------------------------------
        // GET PARENT NAME
        // --------------------------------------------------------

        FString ParentComponentName;

        if (!ComponentObject->TryGetStringField(
            TEXT("parent"),
            ParentComponentName))
        {
            // Also accept "attach_to" as an alias.
            ComponentObject->TryGetStringField(
                TEXT("attach_to"),
                ParentComponentName
            );
        }

        ParentComponentName.TrimStartAndEndInline();

        // No parent means the component remains at the root.
        if (ParentComponentName.IsEmpty())
        {
            continue;
        }

        // --------------------------------------------------------
        // FIND PARENT
        // --------------------------------------------------------

        USCS_Node** ParentNodePtr =
            ComponentNodes.Find(FName(*ParentComponentName));

        if (ParentNodePtr && *ParentNodePtr)
        {
            USCS_Node* ParentNode = *ParentNodePtr;

            if (ParentNode != ChildNode)
            {
                // Explicitly establish the parent relationship.
                ChildNode->SetParent(ParentNode);

                // Add the child to the parent's child list.
                ParentNode->AddChildNode(ChildNode, false);

                UE_LOG(
                    LogTemp,
                    Log,
                    TEXT("LocalStudio BlueprintBuilder: Attached component '%s' to parent '%s'."),
                    *ComponentName,
                    *ParentComponentName
                );
            }
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Parent component '%s' was not found for '%s'. Component remains at root."),
                *ParentComponentName,
                *ComponentName
            );
        }
    }

    // ============================================================
    // MARK BLUEPRINT STRUCTURE MODIFIED
    // ============================================================

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
        Blueprint
    );

    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessFunctions(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& FunctionsAdded, int32& FunctionsSkipped, int32& FunctionsAlreadyExisting)
{
    // ============================================================
    // PROCESS BLUEPRINT FUNCTIONS
    // ============================================================

    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* FunctionsArray = nullptr;

    if (!ParsedData->TryGetArrayField(
        TEXT("functions"),
        FunctionsArray))
    {
        // No functions requested is a valid result.
        return true;
    }

    // ============================================================
    // LOCAL PIN TYPE HELPER
    // ============================================================

    auto BuildPinType =
        [](const FString& InType, FEdGraphPinType& OutPinType) -> bool
        {
            FString PinType = InType.ToLower();

            // ----------------------------------------------------
            // BOOL
            // ----------------------------------------------------

            if (PinType == TEXT("bool") ||
                PinType == TEXT("boolean"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Boolean;

                return true;
            }

            // ----------------------------------------------------
            // BYTE
            // ----------------------------------------------------

            if (PinType == TEXT("byte") ||
                PinType == TEXT("uint8"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Byte;

                return true;
            }

            // ----------------------------------------------------
            // INT
            // ----------------------------------------------------

            if (PinType == TEXT("int") ||
                PinType == TEXT("int32") ||
                PinType == TEXT("integer"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Int;

                return true;
            }

            // ----------------------------------------------------
            // INT64
            // ----------------------------------------------------

            if (PinType == TEXT("int64") ||
                PinType == TEXT("integer64"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Int64;

                return true;
            }

            // ----------------------------------------------------
            // FLOAT
            // ----------------------------------------------------

            if (PinType == TEXT("float"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Real;

                OutPinType.PinSubCategory =
                    UEdGraphSchema_K2::PC_Float;

                return true;
            }

            // ----------------------------------------------------
            // DOUBLE
            // ----------------------------------------------------

            if (PinType == TEXT("double"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Real;

                OutPinType.PinSubCategory =
                    UEdGraphSchema_K2::PC_Double;

                return true;
            }

            // ----------------------------------------------------
            // STRING
            // ----------------------------------------------------

            if (PinType == TEXT("string"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_String;

                return true;
            }

            // ----------------------------------------------------
            // TEXT
            // ----------------------------------------------------

            if (PinType == TEXT("text"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Text;

                return true;
            }

            // ----------------------------------------------------
            // NAME
            // ----------------------------------------------------

            if (PinType == TEXT("name"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Name;

                return true;
            }

            // ----------------------------------------------------
            // VECTOR
            // ----------------------------------------------------

            if (PinType == TEXT("vector"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Struct;

                OutPinType.PinSubCategoryObject =
                    TBaseStructure<FVector>::Get();

                return true;
            }

            // ----------------------------------------------------
            // VECTOR 2D
            // ----------------------------------------------------

            if (PinType == TEXT("vector2d"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Struct;

                OutPinType.PinSubCategoryObject =
                    TBaseStructure<FVector2D>::Get();

                return true;
            }

            // ----------------------------------------------------
            // ROTATOR
            // ----------------------------------------------------

            if (PinType == TEXT("rotator"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Struct;

                OutPinType.PinSubCategoryObject =
                    TBaseStructure<FRotator>::Get();

                return true;
            }

            // ----------------------------------------------------
            // TRANSFORM
            // ----------------------------------------------------

            if (PinType == TEXT("transform"))
            {
                OutPinType.PinCategory =
                    UEdGraphSchema_K2::PC_Struct;

                OutPinType.PinSubCategoryObject =
                    TBaseStructure<FTransform>::Get();

                return true;
            }

            return false;
        };

    // ============================================================
    // PROCESS EACH FUNCTION
    // ============================================================

    for (const TSharedPtr<FJsonValue>& FunctionValue : *FunctionsArray)
    {
        if (!FunctionValue.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> FunctionObject =
            FunctionValue->AsObject();

        if (!FunctionObject.IsValid())
        {
            continue;
        }

        // ========================================================
        // FUNCTION NAME
        // ========================================================

        FString FunctionName;

        if (!FunctionObject->TryGetStringField(
            TEXT("function_name"),
            FunctionName))
        {
            // Accept the alternate schema used by the LLM.
            FunctionObject->TryGetStringField(
                TEXT("name"),
                FunctionName);
        }

        if (FunctionName.IsEmpty())
        {
            continue;
        }

        FName FunctionFName(*FunctionName);

        // ========================================================
        // CHECK EXISTING BLUEPRINT FUNCTION
        // ========================================================

        bool bFunctionExists = false;
        bool bNameConflict = false;

        for (UEdGraph* ExistingGraph : Blueprint->FunctionGraphs)
        {
            if (ExistingGraph &&
                ExistingGraph->GetFName() == FunctionFName)
            {
                bFunctionExists = true;
                break;
            }
        }

        // ========================================================
        // CHECK INHERITED C++ FUNCTION
        // ========================================================

        if (!bFunctionExists && ParentClass)
        {
            if (UFunction* ExistingFunction =
                ParentClass->FindFunctionByName(FunctionFName))
            {
                bNameConflict = true;

                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("LocalStudio BlueprintBuilder: Cannot create function '%s'. Name is already used by inherited C++ function '%s' in '%s'."),
                    *FunctionName,
                    *ExistingFunction->GetName(),
                    *ParentClass->GetName()
                );
            }
        }

        // ========================================================
        // EXISTING FUNCTION
        // ========================================================

        if (bFunctionExists)
        {
            FunctionsAlreadyExisting++;

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio BlueprintBuilder: Function '%s' already exists in '%s'. Not creating duplicate."),
                *FunctionName,
                *Blueprint->GetPathName()
            );

            continue;
        }

        // ========================================================
        // NAME CONFLICT
        // ========================================================

        if (bNameConflict)
        {
            FunctionsSkipped++;

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Skipping function '%s' because the name is already used by an inherited C++ function."),
                *FunctionName
            );

            continue;
        }

        // ========================================================
        // CREATE FUNCTION GRAPH
        // ========================================================

        UEdGraph* FunctionGraph =
            FBlueprintEditorUtils::CreateNewGraph(
                Blueprint,
                FunctionFName,
                UEdGraph::StaticClass(),
                UEdGraphSchema_K2::StaticClass()
            );

        if (!FunctionGraph)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Failed to create function graph '%s'."),
                *FunctionName
            );

            FunctionsSkipped++;
            continue;
        }

        // Explicitly specify UFunction so UE can resolve the
        // templated AddFunctionGraph call with nullptr.
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(
            Blueprint,
            FunctionGraph,
            true,
            nullptr
        );

        // ========================================================
        // FIND FUNCTION ENTRY NODE
        // ========================================================

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
                EntryNode->Rename(nullptr, FunctionGraph, REN_NonTransactional);
                FunctionGraph->AddNode(EntryNode);
                EntryNode->AllocateDefaultPins();
            }

        if (!EntryNode)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Failed to find function entry node for '%s'."),
                *FunctionName
            );

            FunctionsSkipped++;
            continue;
        }

        // ========================================================
        // FIND / CREATE FUNCTION RESULT NODE
        //
        // We create this before processing parameters because the
        // current LLM schema can declare output parameters directly
        // inside "parameters" using:
        //     "direction": "Output"
        // ========================================================

        UK2Node_FunctionResult* ResultNode =
            FBlueprintEditorUtils::FindOrCreateFunctionResultNode(
                EntryNode
            );

        if (!ResultNode)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Failed to create function result node for '%s'."),
                *FunctionName
            );

            FunctionsSkipped++;
            continue;
        }
        
        // ========================================================
        // INPUT PINS / PARAMETERS
        //
        // Function inputs are OUTPUT pins on FunctionEntry.
        //
        // Current LLM schema:
        //   "parameters": [
        //       { "name": "A", "type": "float" }
        //   ]
        //
        // Legacy schema:
        //   "input_pins": [
        //       { "pin_name": "A", "pin_type": "Float" }
        //   ]
        // ========================================================

        const TArray<TSharedPtr<FJsonValue>>* InputPinsArray = nullptr;

        bool bHasInputPins =
            FunctionObject->TryGetArrayField(
                TEXT("input_pins"),
                InputPinsArray
            );

        if (bHasInputPins)
        {
            for (const TSharedPtr<FJsonValue>& PinValue :
                *InputPinsArray)
            {
                if (!PinValue.IsValid())
                {
                    continue;
                }

                TSharedPtr<FJsonObject> PinObject =
                    PinValue->AsObject();

                if (!PinObject.IsValid())
                {
                    continue;
                }

                FString PinName;
                FString PinType;

                if (!PinObject->TryGetStringField(
                    TEXT("pin_name"),
                    PinName))
                {
                    PinObject->TryGetStringField(
                        TEXT("name"),
                        PinName);
                }

                if (!PinObject->TryGetStringField(
                    TEXT("pin_type"),
                    PinType))
                {
                    PinObject->TryGetStringField(
                        TEXT("type"),
                        PinType);
                }

                if (PinName.IsEmpty() ||
                    PinType.IsEmpty())
                {
                    continue;
                }

                FEdGraphPinType GraphPinType;

                if (!BuildPinType(
                    PinType,
                    GraphPinType))
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("LocalStudio BlueprintBuilder: Function '%s' input '%s' uses unsupported type '%s'."),
                        *FunctionName,
                        *PinName,
                        *PinType
                    );

                    continue;
                }

                EntryNode->CreateUserDefinedPin(
                    FName(*PinName),
                    GraphPinType,
                    EGPD_Output
                );

                UE_LOG(
                    LogTemp,
                    Log,
                    TEXT("LocalStudio BlueprintBuilder: Added input pin '%s' of type '%s' to function '%s'."),
                    *PinName,
                    *PinType,
                    *FunctionName
                );
            }
        }
        else
        {
            // ----------------------------------------------------
            // CURRENT SCHEMA: parameters
            //
            // Each parameter may specify:
            //   "direction": "Input"
            //   "direction": "Output"
            //
            // Inputs belong on FunctionEntry.
            // Outputs belong on FunctionResult.
            // ----------------------------------------------------

            const TArray<TSharedPtr<FJsonValue>>* ParametersArray =
                nullptr;

            if (FunctionObject->TryGetArrayField(
                TEXT("parameters"),
                ParametersArray))
            {
                for (const TSharedPtr<FJsonValue>& ParameterValue :
                    *ParametersArray)
                {
                    if (!ParameterValue.IsValid())
                    {
                        continue;
                    }

                    TSharedPtr<FJsonObject> ParameterObject =
                        ParameterValue->AsObject();

                    if (!ParameterObject.IsValid())
                    {
                        continue;
                    }

                    FString PinName;
                    FString PinType;
                    FString Direction;

                    ParameterObject->TryGetStringField(
                        TEXT("name"),
                        PinName
                    );

                    ParameterObject->TryGetStringField(
                        TEXT("type"),
                        PinType
                    );

                    ParameterObject->TryGetStringField(
                        TEXT("direction"),
                        Direction
                    );

                    PinName.TrimStartAndEndInline();
                    PinType.TrimStartAndEndInline();
                    Direction.TrimStartAndEndInline();

                    if (PinName.IsEmpty() ||
                        PinType.IsEmpty())
                    {
                        continue;
                    }

                    FEdGraphPinType GraphPinType;

                    if (!BuildPinType(
                        PinType,
                        GraphPinType))
                    {
                        UE_LOG(
                            LogTemp,
                            Warning,
                            TEXT("LocalStudio BlueprintBuilder: Function '%s' parameter '%s' uses unsupported type '%s'."),
                            *FunctionName,
                            *PinName,
                            *PinType
                        );

                        continue;
                    }

                    // ====================================================
                    // OUTPUT PARAMETER
                    // ====================================================

                    if (Direction.Equals(
                        TEXT("Output"),
                        ESearchCase::IgnoreCase) ||
                        Direction.Equals(
                            TEXT("Return"),
                            ESearchCase::IgnoreCase))
                    {
                        ResultNode =
                            FBlueprintEditorUtils::FindOrCreateFunctionResultNode(
                                EntryNode
                            );

                        if (!ResultNode)
                        {
                            UE_LOG(
                                LogTemp,
                                Warning,
                                TEXT("LocalStudio BlueprintBuilder: Could not create FunctionResult node for output parameter '%s' in function '%s'."),
                                *PinName,
                                *FunctionName
                            );

                            continue;
                        }

                        ResultNode->CreateUserDefinedPin(
                            FName(*PinName),
                            GraphPinType,
                            EGPD_Input
                        );

                        UE_LOG(
                            LogTemp,
                            Log,
                            TEXT("LocalStudio BlueprintBuilder: Added output pin '%s' of type '%s' to FunctionResult of '%s'."),
                            *PinName,
                            *PinType,
                            *FunctionName
                        );

                        continue;
                    }

                    // ====================================================
                    // INPUT PARAMETER
                    // ====================================================

                    EntryNode->CreateUserDefinedPin(
                        FName(*PinName),
                        GraphPinType,
                        EGPD_Output
                    );

                    UE_LOG(
                        LogTemp,
                        Log,
                        TEXT("LocalStudio BlueprintBuilder: Added input pin '%s' of type '%s' to FunctionEntry of '%s'."),
                        *PinName,
                        *PinType,
                        *FunctionName
                    );
                }
            }
        }

        // ========================================================
        // OUTPUT PINS / RETURN VALUE
        //
        // Function outputs are INPUT pins on FunctionResult.
        //
        // Current LLM schema:
        //   "return_type": "float"
        //
        // Legacy schema:
        //   "output_pins": [
        //       { "pin_name": "Result", "pin_type": "Float" }
        //   ]
        // ========================================================

        const TArray<TSharedPtr<FJsonValue>>* OutputPinsArray = nullptr;

        bool bHasOutputPins =
            FunctionObject->TryGetArrayField(
                TEXT("output_pins"),
                OutputPinsArray
            );

        if (bHasOutputPins)
        {
            for (const TSharedPtr<FJsonValue>& PinValue :
                *OutputPinsArray)
            {
                if (!PinValue.IsValid())
                {
                    continue;
                }

                TSharedPtr<FJsonObject> PinObject =
                    PinValue->AsObject();

                if (!PinObject.IsValid())
                {
                    continue;
                }

                FString PinName;
                FString PinType;

                if (!PinObject->TryGetStringField(
                    TEXT("pin_name"),
                    PinName))
                {
                    PinObject->TryGetStringField(
                        TEXT("name"),
                        PinName);
                }

                if (!PinObject->TryGetStringField(
                    TEXT("pin_type"),
                    PinType))
                {
                    PinObject->TryGetStringField(
                        TEXT("type"),
                        PinType);
                }

                if (PinName.IsEmpty() ||
                    PinType.IsEmpty())
                {
                    continue;
                }

                FEdGraphPinType GraphPinType;

                if (!BuildPinType(
                    PinType,
                    GraphPinType))
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("LocalStudio BlueprintBuilder: Function '%s' output '%s' uses unsupported type '%s'."),
                        *FunctionName,
                        *PinName,
                        *PinType
                    );

                    continue;
                }

                ResultNode->CreateUserDefinedPin(
                    FName(*PinName),
                    GraphPinType,
                    EGPD_Input
                );

                UE_LOG(
                    LogTemp,
                    Log,
                    TEXT("LocalStudio BlueprintBuilder: Added output pin '%s' of type '%s' to function '%s'."),
                    *PinName,
                    *PinType,
                    *FunctionName
                );
            }
        }
        else
        {
            // ----------------------------------------------------
            // CURRENT SCHEMA: return_type
            //
            // The current schema has a single return value.
            // LocalStudio uses the conventional Blueprint pin name
            // "Result" unless the LLM explicitly provides another
            // output schema.
            // ----------------------------------------------------

            FString ReturnType;

            if (FunctionObject->TryGetStringField(
                TEXT("return_type"),
                ReturnType))
            {
                if (!ReturnType.IsEmpty() &&
                    !ReturnType.Equals(
                        TEXT("void"),
                        ESearchCase::IgnoreCase))
                {
                    FEdGraphPinType GraphPinType;

                    if (BuildPinType(
                        ReturnType,
                        GraphPinType))
                    {
                        ResultNode->CreateUserDefinedPin(
                            FName(TEXT("Result")),
                            GraphPinType,
                            EGPD_Input
                        );

                        UE_LOG(
                            LogTemp,
                            Log,
                            TEXT("LocalStudio BlueprintBuilder: Added return pin 'Result' of type '%s' to function '%s'."),
                            *ReturnType,
                            *FunctionName
                        );
                    }
                    else
                    {
                        UE_LOG(
                            LogTemp,
                            Warning,
                            TEXT("LocalStudio BlueprintBuilder: Function '%s' uses unsupported return type '%s'."),
                            *FunctionName,
                            *ReturnType
                        );
                    }
                }
            }
        }

        // ========================================================
        // RECONSTRUCT ENTRY / RESULT NODES
        // ========================================================

        EntryNode->ReconstructNode();
        ResultNode->ReconstructNode();

        // ========================================================
        // MARK BLUEPRINT STRUCTURALLY MODIFIED
        // ========================================================

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
            Blueprint
        );

        UE_LOG(
            LogTemp,
            Log,
            TEXT("LocalStudio BlueprintBuilder: Successfully configured function signature '%s'."),
            *FunctionName
        );

        FunctionsAdded++;
    }

    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessFunctionNodes(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, int32& NodesAdded, int32& NodesSkipped)
{
    // ============================================================
    // PROCESS FUNCTION GRAPH NODES
    // ============================================================
    NodesAdded = 0;
    NodesSkipped = 0;
    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
    if (!ParsedData->TryGetArrayField(
        TEXT("nodes"),
        NodesArray))
    {
        // No graph nodes requested.
        return true;
    }
    // ============================================================
    // HELPER:
    // Find a function graph by name.
    // ============================================================
    auto FindFunctionGraph =
    [Blueprint](const FString& FunctionName) -> UEdGraph*
    {
        if (!Blueprint || FunctionName.IsEmpty())
        {
            return nullptr;
        }
        const FName FunctionFName(*FunctionName);
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (!Graph)
            {
                continue;
            }
            if (Graph->GetFName() == FunctionFName)
            {
                return Graph;
            }
        }
        return nullptr;
    };
    // ============================================================
    // PROCESS EACH NODE
    // ============================================================
    for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
    {
        if (!NodeValue.IsValid())
        {
            NodesSkipped++;
            continue;
        }
        TSharedPtr<FJsonObject> NodeObject =
        NodeValue->AsObject();
        if (!NodeObject.IsValid())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Graph node entry is not a valid JSON object.")
            );
            NodesSkipped++;
            continue;
        }
        // ========================================================
        // NODE ID
        // ========================================================
        FString NodeId;
        if (!NodeObject->TryGetStringField(
            TEXT("node_id"),
            NodeId))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Graph node is missing 'node_id'.")
            );
            NodesSkipped++;
            continue;
        }
        NodeId.TrimStartAndEndInline();
        if (NodeId.IsEmpty())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Graph node has an empty 'node_id'.")
            );
            NodesSkipped++;
            continue;
        }
        // ========================================================
        // NODE TYPE
        // ========================================================
        FString NodeType;
        if (!NodeObject->TryGetStringField(
            TEXT("node_type"),
            NodeType))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Graph node '%s' is missing 'node_type'."),
                *NodeId
            );
            NodesSkipped++;
            continue;
        }
        NodeType.TrimStartAndEndInline();
        // ========================================================
        // NODE POSITION
        //
        // Position is handled here so every graph node type uses
        // the same JSON position format.
        // ========================================================
        int32 NodePosX = 0;
        int32 NodePosY = 0;
        bool bHasNodePosition = false;
        const TSharedPtr<FJsonObject>* PositionObject = nullptr;
        if (NodeObject->TryGetObjectField(
            TEXT("position"),
            PositionObject) &&
            PositionObject &&
            PositionObject->IsValid())
        {
            double PositionX = 0.0;
            double PositionY = 0.0;
            if ((*PositionObject)->TryGetNumberField(
                TEXT("x"),
                PositionX) &&
                (*PositionObject)->TryGetNumberField(
                TEXT("y"),
                PositionY))
            {
                NodePosX = static_cast<int32>(PositionX);
                NodePosY = static_cast<int32>(PositionY);
                bHasNodePosition = true;
                UE_LOG(
                    LogTemp,
                    Log,
                    TEXT("LocalStudio BlueprintBuilder: Node '%s' requested position X=%d Y=%d."),
                    *NodeId,
                    NodePosX,
                    NodePosY
                );
            }
        }
        // ========================================================
        // FUNCTION NAME
        //
        // Nodes may explicitly provide function_name.
        // ========================================================
        FString FunctionName;
        NodeObject->TryGetStringField(
            TEXT("function_name"),
            FunctionName
        );
        FunctionName.TrimStartAndEndInline();
        // ========================================================
        // FIND FUNCTION GRAPH
        // ========================================================
        UEdGraph* FunctionGraph =
        FindFunctionGraph(FunctionName);
        if (!FunctionGraph)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Could not find function graph '%s' for node '%s'."),
                *FunctionName,
                *NodeId
            );
            NodesSkipped++;
            continue;
        }
        // ========================================================
        // FUNCTION ENTRY / RESULT
        //
        // These nodes are automatically created by Unreal when the
        // Blueprint function is created. Do not create duplicates.
        // ========================================================
        if (NodeType.Equals(
            TEXT("FunctionEntry"),
            ESearchCase::IgnoreCase))
        {
            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio BlueprintBuilder: FunctionEntry node '%s' already exists for function '%s'."),
                *NodeId,
                *FunctionName
            );
            continue;
        }
        if (NodeType.Equals(
            TEXT("FunctionExit"),
            ESearchCase::IgnoreCase) ||
            NodeType.Equals(
            TEXT("FunctionResult"),
            ESearchCase::IgnoreCase))
        {
            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio BlueprintBuilder: FunctionResult node '%s' already exists for function '%s'."),
                *NodeId,
                *FunctionName
            );
            continue;
        }
        if (NodeType.Equals(
            TEXT("Function"),
            ESearchCase::IgnoreCase))
        {
            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio BlueprintBuilder: Function node '%s' represents the already-created function '%s'. No graph node will be created."),
                *NodeId,
                *FunctionName
            );
            continue;
        }
        // ========================================================
        // MATH NODE
        // ========================================================
        if (NodeType.StartsWith(TEXT("Math_")) ||
            NodeType.Equals(
            TEXT("FloatMath"),
            ESearchCase::IgnoreCase))
        {
            if (ProcessMathNode(
                Blueprint,
                FunctionGraph,
                NodeObject,
                NodeId,
                NodeType,
                FunctionName))
            {
                NodesAdded++;
                // ========================================================
                // APPLY NODE POSITION
                //
                // Positioning is handled centrally so every node type
                // follows the same system.
                // ========================================================
                if (bHasNodePosition)
                {
                    UEdGraphNode* CreatedNode = nullptr;
                    for (UEdGraphNode* GraphNode :
                        FunctionGraph->Nodes)
                    {
                        if (!GraphNode)
                        {
                            continue;
                        }
                        if (GraphNode->GetName().Equals(
                            NodeId,
                            ESearchCase::IgnoreCase))
                        {
                            CreatedNode = GraphNode;
                            break;
                        }
                    }
                    if (CreatedNode)
                    {
                        CreatedNode->NodePosX = NodePosX;
                        CreatedNode->NodePosY = NodePosY;
                        CreatedNode->Modify();
                        UE_LOG(
                            LogTemp,
                            Log,
                            TEXT("LocalStudio BlueprintBuilder: Positioned node '%s' at X=%d Y=%d."),
                            *NodeId,
                            NodePosX,
                            NodePosY
                        );
                    }
                    else
                    {
                        UE_LOG(
                            LogTemp,
                            Warning,
                            TEXT("LocalStudio BlueprintBuilder: Could not find created node '%s' to apply position."),
                            *NodeId
                        );
                    }
                }
            }
            else
            {
                NodesSkipped++;
            }
            continue;
        }
        // ========================================================
        // UNSUPPORTED NODE
        // ========================================================
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("LocalStudio BlueprintBuilder: Unsupported graph node type '%s' for node '%s'."),
            *NodeType,
            *NodeId
        );
        NodesSkipped++;
    }
    // ============================================================
    // FINAL GRAPH MODIFICATION
    // ============================================================
    if (NodesAdded > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(
            Blueprint
        );
    }
    return true;
}

bool FLocalStudioBlueprintBuilder::ProcessFunctionConnections(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, int32& ConnectionsMade, int32& ConnectionsSkipped)
{
    // ============================================================
    // PROCESS FUNCTION GRAPH CONNECTIONS
    // ============================================================

    ConnectionsMade = 0;
    ConnectionsSkipped = 0;

    if (!Blueprint || !ParsedData.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray = nullptr;

    if (!ParsedData->TryGetArrayField(
        TEXT("connections"),
        ConnectionsArray))
    {
        return true;
    }

    // ============================================================
    // RECONSTRUCT FUNCTION GRAPH NODES
    //
    // Blueprint nodes may have been loaded from disk or created
    // during the previous processing pass. Reconstructing here
    // ensures the connection pass sees the current pins.
    // ============================================================

    // ============================================================
    // DO NOT RECONSTRUCT ALL NODES HERE
    //
    // Nodes created by ProcessFunctionNodes() have already been
    // initialized and reconstructed before this connection pass.
    //
    // Reconstructing every node again can cause promoted operator
    // pins to be regenerated unnecessarily.
    // ============================================================

    // ============================================================
    // HELPER:
    // Find a graph node by our JSON node ID.
    //
    // Entry/Result nodes are identified by their node type because
    // their Unreal-generated object names are not guaranteed to
    // match our JSON IDs.
    // ============================================================

    auto FindNodeById =
        [](UEdGraph* Graph, const FString& NodeId) -> UEdGraphNode*
        {
            if (!Graph || NodeId.IsEmpty())
            {
                return nullptr;
            }

            // ========================================================
            // DIRECT OBJECT NAME MATCH
            // ========================================================

            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (!Node)
                {
                    continue;
                }

                if (Node->GetName().Equals(
                    NodeId,
                    ESearchCase::IgnoreCase))
                {
                    return Node;
                }
            }

            // ========================================================
            // FUNCTION ENTRY
            //
            // LLM node IDs may use:
            //   FunctionEntry_0
            //   Entry
            //   Function_<FunctionName>
            //
            // Unreal's actual object name is typically:
            //   K2Node_FunctionEntry_0
            // ========================================================

            if (NodeId.Equals(
                TEXT("FunctionEntry_0"),
                ESearchCase::IgnoreCase) ||
                NodeId.Equals(
                    TEXT("Entry"),
                    ESearchCase::IgnoreCase) ||
                NodeId.StartsWith(
                    TEXT("Function_"),
                    ESearchCase::IgnoreCase))
            {
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (Cast<UK2Node_FunctionEntry>(Node))
                    {
                        return Node;
                    }
                }
            }

            // ========================================================
            // FUNCTION RESULT
            //
            // LLM node IDs may use:
            //   FunctionResult_0
            //   FunctionExit_0
            //   Result
            //   FunctionResult_<FunctionName>
            //
            // Unreal's actual object name is typically:
            //   K2Node_FunctionResult_0
            // ========================================================

            if (NodeId.Equals(
                TEXT("FunctionResult_0"),
                ESearchCase::IgnoreCase) ||
                NodeId.Equals(
                    TEXT("FunctionExit_0"),
                    ESearchCase::IgnoreCase) ||
                NodeId.Equals(
                    TEXT("Result"),
                    ESearchCase::IgnoreCase) ||
                NodeId.StartsWith(
                    TEXT("FunctionResult_"),
                    ESearchCase::IgnoreCase))
            {
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (Cast<UK2Node_FunctionResult>(Node))
                    {
                        return Node;
                    }
                }
            }

            return nullptr;
        };

    auto ResolvePinName =
        [](UEdGraphNode* Node, const FString& RequestedPinName) -> FName
        {
            if (!Node || RequestedPinName.IsEmpty())
            {
                return NAME_None;
            }

            FString Requested =
                RequestedPinName;

            Requested.TrimStartAndEndInline();

            // ========================================================
            // DIRECT MATCH
            // ========================================================

            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin)
                {
                    continue;
                }

                if (Pin->PinName.ToString().Equals(
                    Requested,
                    ESearchCase::IgnoreCase))
                {
                    return Pin->PinName;
                }
            }

            // ========================================================
            // RESULT ALIAS
            // ========================================================

            if (Requested.Equals(
                TEXT("Result"),
                ESearchCase::IgnoreCase))
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->PinName ==
                        UEdGraphSchema_K2::PN_ReturnValue)
                    {
                        return Pin->PinName;
                    }
                }
            }

            // ========================================================
            // RETURN VALUE ALIAS
            // ========================================================

            if (Requested.Equals(
                TEXT("Return"),
                ESearchCase::IgnoreCase) ||
                Requested.Equals(
                    TEXT("Output"),
                    ESearchCase::IgnoreCase))
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->PinName ==
                        UEdGraphSchema_K2::PN_ReturnValue)
                    {
                        return Pin->PinName;
                    }
                }
            }

            // ========================================================
            // FUNCTION EXEC PIN ALIASES
            // ========================================================

            if (Requested.Equals(
                TEXT("then"),
                ESearchCase::IgnoreCase))
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->PinName ==
                        UEdGraphSchema_K2::PN_Then)
                    {
                        return Pin->PinName;
                    }
                }
            }

            if (Requested.Equals(
                TEXT("execute"),
                ESearchCase::IgnoreCase))
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->PinName ==
                        UEdGraphSchema_K2::PN_Execute)
                    {
                        return Pin->PinName;
                    }
                }
            }

            return NAME_None;
        };

    // ============================================================
    // PROCESS EACH CONNECTION
    // ============================================================

    for (const TSharedPtr<FJsonValue>& ConnectionValue :
        *ConnectionsArray)
    {
        if (!ConnectionValue.IsValid())
        {
            ConnectionsSkipped++;
            continue;
        }

        TSharedPtr<FJsonObject> ConnectionObject =
            ConnectionValue->AsObject();

        if (!ConnectionObject.IsValid())
        {
            ConnectionsSkipped++;
            continue;
        }

        FString FromNodeId;
        FString FromPinName;
        FString ToNodeId;
        FString ToPinName;
        FString FunctionName;

        if (!ConnectionObject->TryGetStringField(
            TEXT("from_node"),
            FromNodeId))
        {
            ConnectionsSkipped++;
            continue;
        }

        if (!ConnectionObject->TryGetStringField(
            TEXT("from_pin"),
            FromPinName))
        {
            ConnectionsSkipped++;
            continue;
        }

        if (!ConnectionObject->TryGetStringField(
            TEXT("to_node"),
            ToNodeId))
        {
            ConnectionsSkipped++;
            continue;
        }

        if (!ConnectionObject->TryGetStringField(
            TEXT("to_pin"),
            ToPinName))
        {
            ConnectionsSkipped++;
            continue;
        }

        

        ConnectionObject->TryGetStringField(
            TEXT("function_name"),
            FunctionName
        );

        FunctionName.TrimStartAndEndInline();

        // ========================================================
        // DETERMINE FUNCTION GRAPH
        // ========================================================

        UEdGraph* TargetGraph = nullptr;
        UEdGraphNode* FromNode = nullptr;
        UEdGraphNode* ToNode = nullptr;

        // ========================================================
        // PREFERRED:
        // Explicit function_name supplied by LLM.
        // ========================================================

        if (!FunctionName.IsEmpty())
        {
            const FName FunctionFName(*FunctionName);

            for (UEdGraph* FunctionGraph :
                Blueprint->FunctionGraphs)
            {
                if (!FunctionGraph)
                {
                    continue;
                }

                if (FunctionGraph->GetFName() != FunctionFName)
                {
                    continue;
                }

                TargetGraph = FunctionGraph;

                FromNode =
                    FindNodeById(
                        FunctionGraph,
                        FromNodeId
                    );

                ToNode =
                    FindNodeById(
                        FunctionGraph,
                        ToNodeId
                    );

                break;
            }
        }

        // ========================================================
        // FALLBACK:
        // Search all function graphs.
        //
        // This allows older JSON without function_name to
        // continue working when node IDs are unique.
        // ========================================================

        if (!TargetGraph)
        {
            for (UEdGraph* FunctionGraph :
                Blueprint->FunctionGraphs)
            {
                if (!FunctionGraph)
                {
                    continue;
                }

                UEdGraphNode* CandidateFrom =
                    FindNodeById(
                        FunctionGraph,
                        FromNodeId
                    );

                UEdGraphNode* CandidateTo =
                    FindNodeById(
                        FunctionGraph,
                        ToNodeId
                    );

                if (CandidateFrom &&
                    CandidateTo)
                {
                    TargetGraph = FunctionGraph;
                    FromNode = CandidateFrom;
                    ToNode = CandidateTo;
                    break;
                }
            }
        }

        if (!TargetGraph ||
            !FromNode ||
            !ToNode)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Could not resolve connection %s.%s -> %s.%s in function '%s'."),
                *FromNodeId,
                *FromPinName,
                *ToNodeId,
                *ToPinName,
                FunctionName.IsEmpty()
                ? TEXT("<unspecified>")
                : *FunctionName
            );

            ConnectionsSkipped++;
            continue;
        }

        // ========================================================
        // FIND SOURCE PIN
        // ========================================================

        UEdGraphPin* FromPin = nullptr;

        for (UEdGraphPin* Pin :
            FromNode->Pins)
        {
            if (!Pin)
            {
                continue;
            }

            const FName ResolvedFromPinName =
                ResolvePinName(
                    FromNode,
                    FromPinName
                );

            if (ResolvedFromPinName != NAME_None &&
                Pin->PinName == ResolvedFromPinName)
            {
                FromPin = Pin;
                break;
            }
        }

        if (!FromPin)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Source pin '%s' was not found on node '%s'."),
                *FromPinName,
                *FromNodeId
            );

            ConnectionsSkipped++;
            continue;
        }

        // ========================================================
        // FIND DESTINATION PIN
        // ========================================================

        UEdGraphPin* ToPin = nullptr;

        for (UEdGraphPin* Pin :
            ToNode->Pins)
        {
            if (!Pin)
            {
                continue;
            }

            const FName ResolvedToPinName =
                ResolvePinName(
                    ToNode,
                    ToPinName
                );

            if (ResolvedToPinName != NAME_None &&
                Pin->PinName == ResolvedToPinName)
            {
                ToPin = Pin;
                break;
            }
        }

        if (!ToPin)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Destination pin '%s' was not found on node '%s'."),
                *ToPinName,
                *ToNodeId
            );

            ConnectionsSkipped++;
            continue;
        }

        // ========================================================
        // CHECK DIRECTIONS
        // ========================================================

        if (FromPin->Direction != EGPD_Output)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Source pin '%s' on node '%s' is not an output pin."),
                *FromPinName,
                *FromNodeId
            );

            ConnectionsSkipped++;
            continue;
        }

        if (ToPin->Direction != EGPD_Input)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Destination pin '%s' on node '%s' is not an input pin."),
                *ToPinName,
                *ToNodeId
            );

            ConnectionsSkipped++;
            continue;
        }

        // ========================================================
        // ALREADY CONNECTED?
        // ========================================================

        bool bAlreadyConnected = false;

        for (UEdGraphPin* LinkedPin :
            FromPin->LinkedTo)
        {
            if (LinkedPin == ToPin)
            {
                bAlreadyConnected = true;
                break;
            }
        }

        if (bAlreadyConnected)
        {
            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio BlueprintBuilder: Connection already exists: %s.%s -> %s.%s."),
                *FromNodeId,
                *FromPinName,
                *ToNodeId,
                *ToPinName
            );

            continue;
        }

        // ========================================================
        // CONNECT
        // ========================================================

        const UEdGraphSchema_K2* Schema =
            GetDefault<UEdGraphSchema_K2>();

        if (!Schema)
        {
            ConnectionsSkipped++;
            continue;
        }

        UE_LOG(
            LogTemp,
            Log,
            TEXT("LocalStudio BlueprintBuilder: Attempting connection %s.%s -> %s.%s. Source category=%s subcategory=%s direction=%d. Destination category=%s subcategory=%s direction=%d."),
            *FromNodeId,
            *FromPinName,
            *ToNodeId,
            *ToPinName,

            * FromPin->PinType.PinCategory.ToString(),
            * FromPin->PinType.PinSubCategory.ToString(),
            static_cast<int32>(FromPin->Direction),

            * ToPin->PinType.PinCategory.ToString(),
            * ToPin->PinType.PinSubCategory.ToString(),
            static_cast<int32>(ToPin->Direction)
        );

        const FPinConnectionResponse Response =
            Schema->CanCreateConnection(
                FromPin,
                ToPin
            );

        if (Response.Response == CONNECT_RESPONSE_DISALLOW)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Unreal rejected connection %s.%s -> %s.%s. Reason: %s"),
                *FromNodeId,
                *FromPinName,
                *ToNodeId,
                *ToPinName,
                *Response.Message.ToString()
            );

            ConnectionsSkipped++;
            continue;
        }

        if (!Schema->TryCreateConnection(
            FromPin,
            ToPin))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio BlueprintBuilder: Failed to create connection %s.%s -> %s.%s."),
                *FromNodeId,
                *FromPinName,
                *ToNodeId,
                *ToPinName
            );

            ConnectionsSkipped++;
            continue;
        }

        ConnectionsMade++;

        UE_LOG(
            LogTemp,
            Log,
            TEXT("LocalStudio BlueprintBuilder: Connected %s.%s -> %s.%s."),
            *FromNodeId,
            *FromPinName,
            *ToNodeId,
            *ToPinName
        );
    }

    

    // ============================================================
    // MARK BLUEPRINT MODIFIED
    // ============================================================

    if (ConnectionsMade > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(
            Blueprint
        );
    }

    return true;
}

bool FLocalStudioBlueprintBuilder::CreateSquareRootNode(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& NodeId, const TSharedPtr<FJsonObject>& NodeObject, const FString& FunctionName)
{
    if (!Blueprint || !FunctionGraph || !NodeObject.IsValid())
    {
        return false;
    }

    // Create a proper math node for square root
    UK2Node_CallFunction* SquareRootNode = NewObject<UK2Node_CallFunction>(FunctionGraph, NAME_None, RF_Transactional);
    if (SquareRootNode)
    {
        // Use the correct function reference for square root
        SquareRootNode->FunctionReference.SetExternalMember(FName(TEXT("Sqrt_Double")), UKismetMathLibrary::StaticClass());
        
        FunctionGraph->AddNode(SquareRootNode, true, false);
        SquareRootNode->Rename(*NodeId, FunctionGraph, REN_DontCreateRedirectors | REN_NonTransactional);
        SquareRootNode->PostPlacedNewNode();
        SquareRootNode->AllocateDefaultPins();
        SquareRootNode->ReconstructNode();
        
        // Set position if specified in the JSON - FIXED: Use FString instead of TEXT macro
        TSharedPtr<FJsonObject> PositionObject;
        if (NodeObject->TryGetObjectField(FString(TEXT("position")), PositionObject))
        {
            int32 X = 0, Y = 0;
            PositionObject->TryGetNumberField(TEXT("x"), X);
            PositionObject->TryGetNumberField(TEXT("y"), Y);
            SquareRootNode->NodePosX = X;
            SquareRootNode->NodePosY = Y;
        }
        
        // Log that we successfully created the node - FIXED: Use FString for FunctionName
        UE_LOG(LogTemp, Log, TEXT("LocalStudio BlueprintBuilder: Created square root node '%s' for function '%s'."), *NodeId, *FunctionName);
        
        // Mark everything as modified
        FunctionGraph->Modify();
        SquareRootNode->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        
        return true;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("LocalStudio BlueprintBuilder: Failed to create square root node '%s'."), *NodeId);
    return false;
}