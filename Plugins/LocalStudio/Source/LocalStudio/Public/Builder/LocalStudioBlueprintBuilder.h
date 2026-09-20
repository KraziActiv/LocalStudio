// LocalStudioBlueprintBuilder.h
#pragma once

#include "CoreMinimal.h"

#include "Core/ILocalStudioBuilder.h"

struct FLocalStudioNodePosition
{
    bool bHasPosition = false;
    int32 X = 0;
    int32 Y = 0;
};

class LOCALSTUDIO_API FLocalStudioBlueprintBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "Blueprint"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "BlueprintMode", "Blueprint Graph Generator"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "BlueprintHint", "Describe Blueprint logic (e.g., 'Door lock actor with key inventory check')..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are an Unreal Engine Blueprint Scripter. Generate Blueprint node graph structures in JSON."); }
    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;

    virtual FString PreparePromptContext(const FString& RawUserPrompt) override;

    UClass* ResolveParentClass(const FString& InClassName);
    static bool ResolveEdGraphPinType(const FString& InTypeStr, const FString& InContainerStr, FEdGraphPinType& OutPinType);
    static bool ProcessPromotableOperatorNode(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& NodeId, const FName& OperationName, const FName& MathLibraryFunctionName, const FString& FunctionName);

    bool ProcessComponents(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& ComponentsAdded, int32& ComponentsSkipped, int32& ComponentsAlreadyExisting);
    bool ProcessVariables(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& VariablesAdded, int32& VariablesSkipped, int32& VariablesAlreadyExisting);
    bool ProcessFunctions(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, UClass* ParentClass, int32& FunctionsAdded, int32& FunctionsSkipped, int32& FunctionsAlreadyExisting);
    bool ProcessFunctionNodes(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, int32& NodesAdded, int32& NodesSkipped);
    bool ProcessFunctionConnections(UBlueprint* Blueprint, TSharedPtr<FJsonObject> ParsedData, int32& ConnectionsMade, int32& ConnectionsSkipped);
    bool ProcessMathNode(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const TSharedPtr<FJsonObject>& NodeObject, const FString& NodeId, const FString& NodeType, const FString& FunctionName);
    bool CreateSquareRootNode(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& NodeId, const TSharedPtr<FJsonObject>& NodeObject);

};