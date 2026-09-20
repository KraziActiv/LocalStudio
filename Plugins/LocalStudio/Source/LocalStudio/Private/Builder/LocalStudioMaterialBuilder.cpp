#include "Builder/LocalStudioMaterialBuilder.h"

#include "Core/StructEnums.h"
#include "Core/LocalStudioAssetUtils.h"
#include "Core/BuilderRegistry.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Core Material Expressions
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"

FString FLocalStudioMaterialBuilder::GetTechnicalGuardrails() const
{
    return TEXT(R"(
[SYSTEM INSTRUCTION - DO NOT EXPOSE TO USER]
You must output a single, valid JSON object matching one of the schemas below.

=== SCHEMA 1: STANDALONE BASE MATERIAL (STATIC & DYNAMIC) ===
Use this when creating a new base material. Supports standard PBR, dynamic pulsing, flashing, fresnel rim lighting, panning textures, and material function calls:
{
  "material_action": {
    "name": "M_PulsingNeon",
    "package_path": "/Game/LocalStudio/Materials/Material",
    "is_instance": false,
    "base_color": [0.1, 0.1, 0.1, 1.0],
    "metallic": 0.0,
    "roughness": 0.2,
    "specular": 0.5,
    "emissive_color": [0.0, 1.0, 0.8, 1.0],
    "emissive_intensity": 5.0,
    "is_pulsing": true,
    "pulse_speed": 2.0,
    "is_flashing": true,
    "flash_frequency": 3.0,
    "flash_colors": [
      [1.0, 0.0, 0.0, 1.0],
      [0.0, 1.0, 0.0, 1.0],
      [0.0, 0.0, 1.0, 1.0]
    ],
    "enable_fresnel": true,
    "fresnel_exponent": 3.0,
    "fresnel_color": [0.0, 1.0, 1.0, 1.0],
    "panning_speed": [0.1, 0.0]
  }
}

=== SCHEMA 2: CREATE NEW MATERIAL FUNCTION ASSET ===
Use ONLY when creating a new Material Function asset (class UMaterialFunction). Always include required math expressions and connections:
{
  "material_action": {
    "name": "MF_LocalStudio_WorldTriplanar",
    "package_path": "/Game/LocalStudio/Materials/Functions",
    "is_material_function": true,
    "description": "Calculates world-space tiled UV coordinates",
    "inputs": [
      { "name": "Tiling", "type": "Scalar", "default_value": 0.005, "pos_x": -600, "pos_y": 0 }
    ],
    "outputs": [
      { "name": "WorldUVs", "pos_x": 400, "pos_y": 0 }
    ],
    "expressions": [
      { "id": "WorldPos", "type": "WorldPosition", "pos_x": -300, "pos_y": 150 },
      { "id": "MultUVs", "type": "Multiply", "pos_x": 0, "pos_y": 0 }
    ],
    "connections": [
      { "from_node": "Tiling", "to_node": "MultUVs", "to_pin": "A" },
      { "from_node": "WorldPos", "to_node": "MultUVs", "to_pin": "B" },
      { "from_node": "MultUVs", "to_node": "WorldUVs", "to_pin": "Input" }
    ]
  }
}

=== SCHEMA 3: MATERIAL INSTANCE (DERIVED) ===
Use this when deriving from an existing material:
{
  "material_action": {
    "name": "MI_GlowingRed",
    "package_path": "/Game/LocalStudio/Materials/MaterialInstance",
    "is_instance": true,
    "parent_material": "M_BaseMaterial",
    "emissive_color": [5.0, 0.0, 0.0, 1.0],
    "pulse_speed": 4.0
  }
}

=== SCHEMA 4: AUTO-LANDSCAPE MATERIAL ===
{
  "material_action": {
    "name": "M_AutoTerrain",
    "package_path": "/Game/LocalStudio/Materials/Landscape",
    "is_auto_landscape": true,
    "layers": [
      { "name": "Beach", "color": [0.76, 0.70, 0.50, 1.0], "max_height": -200.0 },
      { "name": "Grass", "color": [0.15, 0.45, 0.08, 1.0], "max_height": 4000.0 },
      { "name": "RockSlope", "color": [0.25, 0.25, 0.25, 1.0], "is_slope": true, "min_slope_angle": 45.0 },
      { "name": "Snow", "color": [0.95, 0.95, 0.98, 1.0], "min_height": 8000.0 }
    ]
  }
}

=== SCHEMA 5: MATERIAL FUNCTION CALL INCLUSION ===
Use this when inserting reusable material functions into a base material:
{
  "material_action": {
    "name": "M_TestTriplanarUsage",
    "package_path": "/Game/LocalStudio/Materials",
    "is_instance": false,
    "function_calls": [
      {
        "function_path": "/Game/LocalStudio/Materials/MaterialFunction/MF_LocalStudio_WorldTriplanar",
        "pos_x": -600,
        "pos_y": 0,
        "inputs": {
          "Tiling": 5.0
        }
      }
    ]
  }
}

CRITICAL RULES:
- Set 'is_pulsing': true and 'pulse_speed' for smooth sine-wave breathing/glowing effects.
- Set 'is_flashing': true and 'flash_frequency' for sharp strobing/blinking warning lights.
- Set 'enable_fresnel': true for rim highlight effects (shields, glowing edges, holograms).
- Prefix base materials with 'M_', instances with 'MI_', and functions with 'MF_'.
- If user requests to "create a material function", ALWAYS set 'is_material_function': true.
- When 'is_flashing' or 'is_pulsing' is true, NEVER set 'emissive_color' to black [0,0,0,1]. Always provide a visible color.
)");
}

bool FLocalStudioMaterialBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    FString CleanJson = RawResponse.TrimStartAndEnd();
    if (CleanJson.StartsWith(TEXT("```")))
    {
        int32 FirstNewline = CleanJson.Find(TEXT("\n"));
        int32 LastMarker = CleanJson.Find(TEXT("```"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (FirstNewline != INDEX_NONE && LastMarker != INDEX_NONE && LastMarker > FirstNewline)
        {
            CleanJson = CleanJson.Mid(FirstNewline + 1, LastMarker - FirstNewline - 1).TrimStartAndEnd();
        }
    }

    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanJson);
    TSharedPtr<FJsonObject> RootObject;

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutError = TEXT("Failed to parse Material JSON from LLM response.");
        return false;
    }

    if (RootObject->HasTypedField<EJson::Object>(TEXT("material_action")))
    {
        OutParsedData = RootObject->GetObjectField(TEXT("material_action"));
        return true;
    }

    OutParsedData = RootObject;
    return true;
}

FBuilderExecutionResult FLocalStudioMaterialBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;

    if (!ParsedData.IsValid())
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Invalid material parameters.");
        return Result;
    }

    FString TargetAssetName = ParsedData->HasTypedField<EJson::String>(TEXT("name"))
        ? ParsedData->GetStringField(TEXT("name")) : TEXT("M_GeneratedMaterial");
    FString PackagePath = ParsedData->HasTypedField<EJson::String>(TEXT("package_path"))
        ? ParsedData->GetStringField(TEXT("package_path")) : TEXT("/Game/Materials");

    bool bWasCreated = false;
    UObject* CreatedMaterial = CreateMaterialFromJSON(TargetAssetName, PackagePath, ParsedData, bWasCreated);

    if (CreatedMaterial)
    {
        Result.bSuccess = true;

        FString ActualAssetName = CreatedMaterial->GetName();
        FString ActualFolder = CreatedMaterial->GetOutermost()->GetName();
        ActualFolder = FPaths::GetPath(ActualFolder);

        FString ActionVerb = bWasCreated ? TEXT("created") : TEXT("updated");

        Result.UserSummary = FString::Printf(TEXT("Successfully %s asset [%s] located in %s"),
            *ActionVerb, *ActualAssetName, *ActualFolder);
    }
    else
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Failed to create or update material asset.");
    }

    return Result;
}

UObject* FLocalStudioMaterialBuilder::CreateMaterialFromJSON(const FString& TargetAssetName, const FString& PackagePath, TSharedPtr<FJsonObject> MaterialJson, bool& bOutWasCreated)
{
    bOutWasCreated = false;
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

    // ==========================================
    // PATH 0: MATERIAL FUNCTION CREATION
    // ==========================================
    bool bIsMaterialFunction = false;
    MaterialJson->TryGetBoolField(TEXT("is_material_function"), bIsMaterialFunction);

    if (bIsMaterialFunction)
    {
        UMaterialFunctionFactoryNew* FunctionFactory = NewObject<UMaterialFunctionFactoryNew>();

        UMaterialFunction* TargetMF = FLocalStudioAssetUtils::FindOrCreateAsset<UMaterialFunction>(
            TargetAssetName, PackagePath, FunctionFactory, bOutWasCreated
        );

        if (!TargetMF)
        {
            UE_LOG(LogTemp, Error, TEXT("LocalStudio: Failed to create Material Function '%s'"), *TargetAssetName);
            return nullptr;
        }

        // Clean existing expressions for fresh generation
        for (UMaterialExpression* OldExpr : TargetMF->GetExpressionCollection().Expressions)
        {
            if (OldExpr)
            {
                OldExpr->MarkAsGarbage();
            }
        }
        TargetMF->GetExpressionCollection().Empty();

        TMap<FString, UMaterialExpression*> CreatedExpressions;

        // 1. Process Inputs
        const TArray<TSharedPtr<FJsonValue>>* InputsArray;
        if (MaterialJson->TryGetArrayField(TEXT("inputs"), InputsArray))
        {
            int32 InputIdx = 0;
            for (const auto& InputVal : *InputsArray)
            {
                TSharedPtr<FJsonObject> InputObj = InputVal->AsObject();
                if (!InputObj.IsValid()) continue;

                auto* InputNode = NewObject<UMaterialExpressionFunctionInput>(TargetMF);
                FString InputName = InputObj->GetStringField(TEXT("name"));
                InputNode->InputName = FName(*InputName);

                FString TypeStr;
                if (InputObj->TryGetStringField(TEXT("type"), TypeStr))
                {
                    TypeStr = TypeStr.ToLower();
                    if (TypeStr == TEXT("scalar") || TypeStr == TEXT("float"))
                        InputNode->InputType = EFunctionInputType::FunctionInput_Scalar;
                    else if (TypeStr == TEXT("vector2"))
                        InputNode->InputType = EFunctionInputType::FunctionInput_Vector2;
                    else if (TypeStr == TEXT("vector3"))
                        InputNode->InputType = EFunctionInputType::FunctionInput_Vector3;
                    else if (TypeStr == TEXT("vector4"))
                        InputNode->InputType = EFunctionInputType::FunctionInput_Vector4;
                    else if (TypeStr == TEXT("texture2d"))
                        InputNode->InputType = EFunctionInputType::FunctionInput_Texture2D;
                }
                else
                {
                    InputNode->InputType = EFunctionInputType::FunctionInput_Scalar;
                }

                double DefaultVal = 0.0;
                if (InputObj->TryGetNumberField(TEXT("default_value"), DefaultVal) ||
                    InputObj->TryGetNumberField(TEXT("value"), DefaultVal))
                {
                    float Val = static_cast<float>(DefaultVal);
                    InputNode->PreviewValue = FVector4f(Val, Val, Val, Val);
                    InputNode->bUsePreviewValueAsDefault = true;
                }

                int32 NodeX = -600;
                int32 NodeY = InputIdx * 150;
                InputObj->TryGetNumberField(TEXT("pos_x"), NodeX);
                InputObj->TryGetNumberField(TEXT("pos_y"), NodeY);
                InputNode->MaterialExpressionEditorX = NodeX;
                InputNode->MaterialExpressionEditorY = NodeY;

                TargetMF->GetExpressionCollection().AddExpression(InputNode);
                CreatedExpressions.Add(InputName, InputNode);
                InputIdx++;
            }
        }

        // 2. Process Outputs
        const TArray<TSharedPtr<FJsonValue>>* OutputsArray;
        if (MaterialJson->TryGetArrayField(TEXT("outputs"), OutputsArray))
        {
            int32 OutputIdx = 0;
            for (const auto& OutputVal : *OutputsArray)
            {
                TSharedPtr<FJsonObject> OutputObj = OutputVal->AsObject();
                if (!OutputObj.IsValid()) continue;

                auto* OutputNode = NewObject<UMaterialExpressionFunctionOutput>(TargetMF);
                FString OutputName = OutputObj->GetStringField(TEXT("name"));
                OutputNode->OutputName = FName(*OutputName);

                int32 NodeX = 600;
                int32 NodeY = OutputIdx * 150;
                OutputObj->TryGetNumberField(TEXT("pos_x"), NodeX);
                OutputObj->TryGetNumberField(TEXT("pos_y"), NodeY);
                OutputNode->MaterialExpressionEditorX = NodeX;
                OutputNode->MaterialExpressionEditorY = NodeY;

                TargetMF->GetExpressionCollection().AddExpression(OutputNode);
                CreatedExpressions.Add(OutputName, OutputNode);
                OutputIdx++;
            }
        }

        // 3. Process General Expressions
        const TArray<TSharedPtr<FJsonValue>>* ExpressionsArray;
        if (MaterialJson->TryGetArrayField(TEXT("expressions"), ExpressionsArray) ||
            MaterialJson->TryGetArrayField(TEXT("nodes"), ExpressionsArray))
        {
            for (const auto& ExprVal : *ExpressionsArray)
            {
                TSharedPtr<FJsonObject> ExprObj = ExprVal->AsObject();
                if (!ExprObj.IsValid()) continue;

                FString ExprType = ExprObj->GetStringField(TEXT("type"));
                FString NodeId = ExprObj->GetStringField(TEXT("id"));
                if (NodeId.IsEmpty()) NodeId = ExprType;

                UMaterialExpression* NewExpr = nullptr;

                if (ExprType.Equals(TEXT("WorldPosition"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionWorldPosition>(TargetMF);
                else if (ExprType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionMultiply>(TargetMF);
                else if (ExprType.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionAdd>(TargetMF);
                else if (ExprType.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionSubtract>(TargetMF);
                else if (ExprType.Equals(TEXT("Divide"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionDivide>(TargetMF);
                else if (ExprType.Equals(TEXT("Clamp"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionClamp>(TargetMF);
                else if (ExprType.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) || ExprType.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase))
                {
                    auto* ConstNode = NewObject<UMaterialExpressionConstant>(TargetMF);
                    double Val = 0.0;
                    if (ExprObj->TryGetNumberField(TEXT("value"), Val)) ConstNode->R = static_cast<float>(Val);
                    NewExpr = ConstNode;
                }
                else if (ExprType.Equals(TEXT("ComponentMask"), ESearchCase::IgnoreCase) || ExprType.Equals(TEXT("Mask"), ESearchCase::IgnoreCase))
                {
                    auto* MaskNode = NewObject<UMaterialExpressionComponentMask>(TargetMF);
                    MaskNode->R = ExprObj->HasField(TEXT("r")) ? ExprObj->GetBoolField(TEXT("r")) : 1;
                    MaskNode->G = ExprObj->HasField(TEXT("g")) ? ExprObj->GetBoolField(TEXT("g")) : 1;
                    MaskNode->B = ExprObj->HasField(TEXT("b")) ? ExprObj->GetBoolField(TEXT("b")) : 0;
                    MaskNode->A = ExprObj->HasField(TEXT("a")) ? ExprObj->GetBoolField(TEXT("a")) : 0;
                    NewExpr = MaskNode;
                }
                else if (ExprType.Equals(TEXT("LinearInterpolate"), ESearchCase::IgnoreCase) || ExprType.Equals(TEXT("Lerp"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionLinearInterpolate>(TargetMF);
                else if (ExprType.Equals(TEXT("DotProduct"), ESearchCase::IgnoreCase) || ExprType.Equals(TEXT("Dot"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionDotProduct>(TargetMF);
                else if (ExprType.Equals(TEXT("Sine"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionSine>(TargetMF);
                else if (ExprType.Equals(TEXT("Frac"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionFrac>(TargetMF);
                else if (ExprType.Equals(TEXT("Floor"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionFloor>(TargetMF);
                else if (ExprType.Equals(TEXT("Fresnel"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionFresnel>(TargetMF);
                else if (ExprType.Equals(TEXT("VertexNormalWS"), ESearchCase::IgnoreCase))
                    NewExpr = NewObject<UMaterialExpressionVertexNormalWS>(TargetMF);
                else
                {
                    FString ClassName = FString::Printf(TEXT("MaterialExpression%s"), *ExprType);
                    UClass* FoundClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
                    if (FoundClass && FoundClass->IsChildOf(UMaterialExpression::StaticClass()))
                    {
                        NewExpr = NewObject<UMaterialExpression>(TargetMF, FoundClass);
                    }
                }

                if (NewExpr)
                {
                    int32 NodeX = 0, NodeY = 0;
                    ExprObj->TryGetNumberField(TEXT("pos_x"), NodeX);
                    ExprObj->TryGetNumberField(TEXT("pos_y"), NodeY);
                    NewExpr->MaterialExpressionEditorX = NodeX;
                    NewExpr->MaterialExpressionEditorY = NodeY;

                    TargetMF->GetExpressionCollection().AddExpression(NewExpr);
                    CreatedExpressions.Add(NodeId, NewExpr);
                }
            }
        }

        // 4. Connect Expression Pins
        const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray;
        if (MaterialJson->TryGetArrayField(TEXT("connections"), ConnectionsArray))
        {
            for (const auto& ConnVal : *ConnectionsArray)
            {
                TSharedPtr<FJsonObject> ConnObj = ConnVal->AsObject();
                if (!ConnObj.IsValid()) continue;

                FString FromNodeKey = ConnObj->GetStringField(TEXT("from_node"));
                FString ToNodeKey = ConnObj->GetStringField(TEXT("to_node"));
                FString ToPin = ConnObj->GetStringField(TEXT("to_pin"));

                UMaterialExpression** FromExprPtr = CreatedExpressions.Find(FromNodeKey);
                UMaterialExpression** ToExprPtr = CreatedExpressions.Find(ToNodeKey);

                if (FromExprPtr && ToExprPtr && *FromExprPtr && *ToExprPtr)
                {
                    UMaterialExpression* FromExpr = *FromExprPtr;
                    UMaterialExpression* ToExpr = *ToExprPtr;

                    if (auto* OutputNode = Cast<UMaterialExpressionFunctionOutput>(ToExpr))
                    {
                        OutputNode->A.Expression = FromExpr;
                        continue;
                    }

                    bool bConnected = false;
                    for (FProperty* Prop = ToExpr->GetClass()->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
                    {
                        if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
                        {
                            FString StructName = StructProp->Struct ? StructProp->Struct->GetName() : TEXT("");
                            if (StructName.Contains(TEXT("ExpressionInput")) || StructName.Contains(TEXT("MaterialInput")))
                            {
                                if (StructProp->GetName().Equals(ToPin, ESearchCase::IgnoreCase) ||
                                    ToPin.Equals(TEXT("Input"), ESearchCase::IgnoreCase))
                                {
                                    FExpressionInput* InputPtr = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpr);
                                    if (InputPtr)
                                    {
                                        InputPtr->Expression = FromExpr;
                                        bConnected = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        TargetMF->PreEditChange(nullptr);
        TargetMF->PostEditChange();
        TargetMF->MarkPackageDirty();

        return TargetMF;
    }

    // ==========================================
    // PATH A: MATERIAL INSTANCE CREATION
    // ==========================================
    bool bIsInstance = false;
    MaterialJson->TryGetBoolField(TEXT("is_instance"), bIsInstance);

    if (bIsInstance || MaterialJson->HasField(TEXT("parent_material")))
    {
        FString ParentMaterialName;
        MaterialJson->TryGetStringField(TEXT("parent_material"), ParentMaterialName);

        UMaterialInterface* ParentMat = FLocalStudioAssetUtils::FindAssetByName<UMaterialInterface>(ParentMaterialName);

        if (!ParentMat)
        {
            UE_LOG(LogTemp, Error, TEXT("LocalStudio: Could not find Parent Material '%s' anywhere in the project."), *ParentMaterialName);
            return nullptr;
        }

        UMaterialInstanceConstantFactoryNew* MICFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
        UMaterialInstanceConstant* TargetMIC = FLocalStudioAssetUtils::FindOrCreateAsset<UMaterialInstanceConstant>(
            TargetAssetName, PackagePath, MICFactory, bOutWasCreated
        );

        if (!TargetMIC) return nullptr;

        TargetMIC->SetParentEditorOnly(ParentMat);

        auto SetVectorParam = [&](const FString& JsonKey, FName ParamName)
            {
                const TArray<TSharedPtr<FJsonValue>>* ColorArray;
                if (MaterialJson->TryGetArrayField(JsonKey, ColorArray) && ColorArray->Num() >= 3)
                {
                    FLinearColor ColorVal((*ColorArray)[0]->AsNumber(), (*ColorArray)[1]->AsNumber(), (*ColorArray)[2]->AsNumber(), 1.0f);
                    if (ColorArray->Num() >= 4) { ColorVal.A = (*ColorArray)[3]->AsNumber(); }
                    TargetMIC->SetVectorParameterValueEditorOnly(ParamName, ColorVal);
                }
            };

        SetVectorParam(TEXT("base_color"), FName(TEXT("BaseColor")));
        SetVectorParam(TEXT("emissive_color"), FName(TEXT("EmissiveColor")));

        auto SetScalarParam = [&](const FString& JsonKey, FName ParamName)
            {
                double Value = 0.0;
                if (MaterialJson->TryGetNumberField(JsonKey, Value))
                {
                    TargetMIC->SetScalarParameterValueEditorOnly(ParamName, static_cast<float>(Value));
                }
            };

        SetScalarParam(TEXT("metallic"), FName(TEXT("Metallic")));
        SetScalarParam(TEXT("roughness"), FName(TEXT("Roughness")));
        SetScalarParam(TEXT("specular"), FName(TEXT("Specular")));

        TargetMIC->PostEditChange();
        TargetMIC->MarkPackageDirty();

        return TargetMIC;
    }

    // ==========================================
    // PATH B: BASE MATERIAL GRAPH CREATION
    // ==========================================
    UMaterial* NewMaterial = FLocalStudioAssetUtils::FindAssetByName<UMaterial>(TargetAssetName);

    if (NewMaterial)
    {
        UE_LOG(LogTemp, Log, TEXT("LocalStudio: Found existing material '%s' at '%s'. Reusing and updating..."),
            *TargetAssetName, *NewMaterial->GetPathName());

        NewMaterial->GetExpressionInputForProperty(MP_BaseColor)->Expression = nullptr;
        NewMaterial->GetExpressionInputForProperty(MP_Metallic)->Expression = nullptr;
        NewMaterial->GetExpressionInputForProperty(MP_Specular)->Expression = nullptr;
        NewMaterial->GetExpressionInputForProperty(MP_Roughness)->Expression = nullptr;
        NewMaterial->GetExpressionInputForProperty(MP_EmissiveColor)->Expression = nullptr;

        NewMaterial->GetExpressionCollection().Empty();
    }
    else
    {
        FString FullPackagePath = PackagePath / TargetAssetName;
        UPackage* Package = CreatePackage(*FullPackagePath);
        if (!Package)
        {
            UE_LOG(LogTemp, Error, TEXT("LocalStudio: Failed to create package at %s"), *FullPackagePath);
            return nullptr;
        }

        UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
        NewMaterial = FLocalStudioAssetUtils::FindOrCreateAssetWithPolicy<UMaterial>(
            TargetAssetName, PackagePath, MaterialFactory, bOutWasCreated, true
        );

        if (!NewMaterial)
        {
            UE_LOG(LogTemp, Error, TEXT("LocalStudio: Factory failed to create UMaterial %s"), *TargetAssetName);
            return nullptr;
        }

        FAssetRegistryModule::AssetCreated(NewMaterial);
    }

    NewMaterial->MarkPackageDirty();

    int32 NodeOffsetY = 0;
    bool bIsAutoLandscape = false;
    MaterialJson->TryGetBoolField(TEXT("is_auto_landscape"), bIsAutoLandscape);

    // Standard Base Material Node Construction
    FLinearColor InitialBaseColor(0.8f, 0.8f, 0.8f, 1.0f);
    const TArray<TSharedPtr<FJsonValue>>* ColorArray;
    if (MaterialJson->TryGetArrayField(TEXT("base_color"), ColorArray) && ColorArray->Num() >= 3)
    {
        InitialBaseColor.R = (*ColorArray)[0]->AsNumber();
        InitialBaseColor.G = (*ColorArray)[1]->AsNumber();
        InitialBaseColor.B = (*ColorArray)[2]->AsNumber();
        if (ColorArray->Num() >= 4) { InitialBaseColor.A = (*ColorArray)[3]->AsNumber(); }
    }

    auto* BaseColorParam = NewObject<UMaterialExpressionVectorParameter>(NewMaterial);
    BaseColorParam->ParameterName = FName("BaseColor");
    BaseColorParam->DefaultValue = InitialBaseColor;
    BaseColorParam->MaterialExpressionEditorX = -600;
    BaseColorParam->MaterialExpressionEditorY = NodeOffsetY;
    NewMaterial->GetExpressionCollection().AddExpression(BaseColorParam);

    NewMaterial->GetExpressionInputForProperty(MP_BaseColor)->Expression = BaseColorParam;

    NewMaterial->PreEditChange(nullptr);
    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();

    return NewMaterial;
}

int32 FLocalStudioMaterialBuilder::FindFunctionInputIndex(UMaterialExpressionMaterialFunctionCall* FunctionCallNode, const FName& InputName)
{
    if (!FunctionCallNode) return INDEX_NONE;

    for (int32 i = 0; i < FunctionCallNode->FunctionInputs.Num(); ++i)
    {
        const FFunctionExpressionInput& FuncInput = FunctionCallNode->FunctionInputs[i];
        if (FuncInput.ExpressionInput && FuncInput.ExpressionInput->InputName == InputName)
        {
            return i;
        }
    }

    return INDEX_NONE;
}

UMaterialExpressionMaterialFunctionCall* FLocalStudioMaterialBuilder::AddMaterialFunctionCall(
    UObject* TargetMaterialOrFunction,
    const FString& FunctionPath,
    int32 NodeX,
    int32 NodeY)
{
    if (!TargetMaterialOrFunction) return nullptr;

    UMaterialFunction* LoadedMF = Cast<UMaterialFunction>(StaticLoadObject(UMaterialFunction::StaticClass(), nullptr, *FunctionPath));
    if (!LoadedMF)
    {
        LoadedMF = FLocalStudioAssetUtils::FindAssetByName<UMaterialFunction>(FunctionPath);
    }

    if (!LoadedMF)
    {
        UE_LOG(LogTemp, Warning, TEXT("LocalStudio: Could not load Material Function at '%s'"), *FunctionPath);
        return nullptr;
    }

    auto* FuncNode = NewObject<UMaterialExpressionMaterialFunctionCall>(TargetMaterialOrFunction);
    FuncNode->SetMaterialFunction(LoadedMF);
    FuncNode->UpdateFromFunctionResource();

    FuncNode->MaterialExpressionEditorX = NodeX;
    FuncNode->MaterialExpressionEditorY = NodeY;

    if (UMaterial* Mat = Cast<UMaterial>(TargetMaterialOrFunction))
    {
        Mat->GetExpressionCollection().AddExpression(FuncNode);
    }
    else if (UMaterialFunction* MF = Cast<UMaterialFunction>(TargetMaterialOrFunction))
    {
        MF->GetExpressionCollection().AddExpression(FuncNode);
    }

    return FuncNode;
}

bool FLocalStudioMaterialBuilder::ConnectToFunctionInput(
    UMaterialExpressionMaterialFunctionCall* FuncNode,
    FName InputName,
    UMaterialExpression* ExpressionToConnect)
{
    if (!FuncNode || !ExpressionToConnect) return false;

    int32 InputIdx = FindFunctionInputIndex(FuncNode, InputName);
    if (InputIdx != INDEX_NONE && InputIdx < FuncNode->FunctionInputs.Num())
    {
        FuncNode->FunctionInputs[InputIdx].Input.Expression = ExpressionToConnect;
        return true;
    }

    return false;
}