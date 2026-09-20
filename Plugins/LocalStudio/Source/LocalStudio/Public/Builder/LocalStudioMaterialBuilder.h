#pragma once

#include "CoreMinimal.h"
#include "COre/ILocalStudioBuilder.h"

// Expressions for Pulsing, Flashing, Panning, and Fresnel
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionAppendVector.h"

class UMaterial;
class UMaterialExpressionMaterialFunctionCall;

class LOCALSTUDIO_API FLocalStudioMaterialBuilder : public ILocalStudioBuilder
{
public:
    virtual FName GetBuilderID() const override { return "Material"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "MaterialMode", "Material Generation"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "MaterialHint", "Describe a material (e.g., 'Pulsing glowing neon cyan', 'Flashing warning red', 'Shield rim effect', 'Post Process Scanner')..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override
    {
        return TEXT("You are an Unreal Engine 5 Material Architect. Generate JSON material property specifications supporting PBR, UI, Post-Process, VFX, and Material Functions.");
    }

    virtual FString GetTechnicalGuardrails() const override;
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;

    UObject* CreateMaterialFromJSON(
        const FString& TargetAssetName,
        const FString& PackagePath,
        TSharedPtr<FJsonObject> MaterialJson,
        bool& bOutWasCreated
    );

    int32 FindFunctionInputIndex(UMaterialExpressionMaterialFunctionCall* FunctionCallNode, const FName& InputName);

    bool ConnectToFunctionInput(
        UMaterialExpressionMaterialFunctionCall* FuncNode,
        FName InputName,
        UMaterialExpression* ExpressionToConnect);

    UMaterialExpressionMaterialFunctionCall* AddMaterialFunctionCall(
        UObject* TargetMaterialOrFunction,
        const FString& FunctionPath,
        int32 NodeX,
        int32 NodeY);
};