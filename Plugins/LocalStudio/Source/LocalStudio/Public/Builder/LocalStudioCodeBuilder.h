#pragma once

#include "CoreMinimal.h"
#include "Core/ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioCodeBuilder : public ILocalStudioBuilder
{
public:
    // --- Builder Identity & Configuration ---
    virtual FName GetBuilderID() const override { return "Code"; }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("LocalStudio", "CodeMode", "C++ Source Generator & Compiler"); }
    virtual FText GetInputHintText() const override { return NSLOCTEXT("LocalStudio", "CodeHint", "Describe C++ Actor, Component, system to create, edit, optimize, or delete..."); }
    virtual FString GetDefaultModel() const override { return "qwen3-coder:30b"; }

    virtual FString GetDefaultJobDescription() const override { return TEXT("You are a Senior Unreal Engine 5 C++ Architect. Read existing project headers and source files, write, update, optimize, or delete C++ files, ensuring proper UE5 macros (UCLASS, UPROPERTY, UFUNCTION) and clean compilation."); }

    // --- Context Enrichment ---
    virtual FString PreparePromptContext(const FString& RawUserPrompt) override;

    // --- Guardrail Management ---
    virtual FString GetTechnicalGuardrails() const override;
    static FString GetMinimalSystemPrompt();

    // --- Execution Pipeline ---
    virtual bool ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError) override;
    virtual FBuilderExecutionResult ExecutePlan(TSharedPtr<FJsonObject> ParsedData) override;

    // --- Operations & Helpers ---
    static FString ScanProjectHeaders();
    static FString ReadSourceFile(const FString& RelativeOrAbsolutePath);
    static bool WriteSourceFile(const FString& RelativeFilePath, const FString& FileContent, FString& OutError);
    static bool DeleteSourceFile(const FString& RelativeFilePath, FString& OutError);
    static bool TriggerLiveCompilation(FString& OutCompileLog);
    static FString FindEngineHeader(const FString& ClassOrTypeName);
    static FString BuildParentClassReflectionContext(const FString& ParentClassName);

private:

    void EnsureBuildDependencies(const FString& Code);

    // Structure to hold reflected property and function names from parent classes
    struct FParentClassReflectionInfo
    {
        TSet<FString> PropertyNames;
        TSet<FString> FunctionNames;
        TSet<FString> RPCNames;
        TSet<FString> ComponentNames;
    };

    static FParentClassReflectionInfo GetParentReflectionInfo(const FString& ParentClassName);
    static void GatherParentClassDataFromHeader(const FString& HeaderContent, const FString& ParentClassName, FParentClassReflectionInfo& OutInfo);


};