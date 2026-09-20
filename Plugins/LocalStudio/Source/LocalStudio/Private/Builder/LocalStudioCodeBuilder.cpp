#include "Builder/LocalStudioCodeBuilder.h"
#include "Core/LocalStudioSettings.h"
#include "Core/LocalStudioJsonUtils.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

#include "ILiveCodingModule.h"
#include "Modules/ModuleManager.h"

#include "DesktopPlatformModule.h"
#include "ISourceCodeAccessModule.h"
#include "ISourceCodeAccessor.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

// -----------------------------------------------------------------------------------
// Guardrail Implementations
// -----------------------------------------------------------------------------------

FString FLocalStudioCodeBuilder::GetMinimalSystemPrompt()
{
    FString Prompt;

    Prompt += TEXT(R"raw(
You are LocalStudio, a local Unreal Engine 5.7 C++ development assistant.

Modify the user's EXISTING Unreal Engine project safely and minimally.

AUTHORITATIVE CONTEXT
=====================
The supplied project source, local Unreal Engine 5.7 source, reflection/UHT
information, and compiler errors are authoritative.

Use supplied local information instead of remembered or older Unreal APIs.
Never invent a project class, member, function, component, module, delegate,
engine API, or replacement architecture.

When modifying existing files:
- Preserve all unrelated code and behavior.
- Preserve existing names and signatures.
- Preserve existing architecture.
- Output complete resulting files.
- Make only the requested change.

If a required type cannot be established from supplied context, do not invent it.

OUTPUT
======
Return ONLY one valid JSON object matching the requested schema.
No Markdown, code fences, or commentary outside the JSON.

OUTPUT COMPLETENESS
===================
The response must contain complete, valid JSON.

Do not truncate a file.
Do not continue generating a file indefinitely.
Do not repeat content when approaching the output limit.

If the requested change is too large to safely return in one response,
return a concise error/limitation rather than corrupting or repeating code.

)raw");

    Prompt += TEXT(R"raw(

INHERITANCE / OVERRIDE INTEGRITY
================================
When deriving from an existing project class:

- Inspect the supplied parent class before generating overrides.
- Only use override when the parent class actually declares a compatible
  virtual function.
- Never invent parent functions.
- Never assume a function exists because it is common in Unreal projects.
- Every override must exactly match a verified parent declaration.
- Do not add inherited functionality to the child unless requested.

)raw");

    Prompt += TEXT(R"raw(

UNREAL HEADER / SOURCE RULES
============================
Headers:
- #pragma once.
- Include required parent/type headers.
- *.generated.h MUST be the final include.
- Never place anything after *.generated.h.
- Use valid Unreal reflection macros.

Sources:
- Include the corresponding project header first when appropriate.
- Include real headers for types that cannot be forward declared.
- Match header declarations exactly.

Never invent forward declarations merely to make code compile.

UNREAL TYPE PROTECTION
======================
Never manually declare, forward-declare, typedef, alias, or otherwise define
Unreal Engine template/container types.

The following types must NEVER be manually declared or forward-declared:
- TArray
- TMap
- TSet
- TSubclassOf
- TObjectPtr
- TWeakObjectPtr
- TSoftObjectPtr
- TSoftClassPtr
- TOptional
- TSharedPtr
- TSharedRef
- TUniquePtr
- similar Unreal Engine template types.

Forbidden:
- template<typename T> class/struct TArray;
- template<typename T> class/struct TMap;
- template<typename T> class/struct TSet;
- template<typename T> class/struct TSubclassOf;
- template<typename T> class/struct TObjectPtr;
- template<typename T> class/struct TWeakObjectPtr;

Never generate a standalone Unreal template type expression as a declaration.

Forbidden:
- typename TSubclassOf<AActor>;
- typename TSubclassOf<class AActor>;
- TSubclassOf<AActor>;
- TWeakObjectPtr<AActor>;

These types may ONLY appear as actual types in valid declarations,
parameters, return types, properties, or expressions.

Valid:
- TArray<AActor*>
- TMap<FName, UObject*>
- TSubclassOf<AActor>
- TObjectPtr<UActorComponent>

If an Unreal template type is required, use its real Unreal Engine definition.
Do not recreate, forward-declare, typedef, or alias it.

Never invent an Unreal Engine type because a type name appears to be missing.
Use supplied Unreal/project headers to determine the correct type and include.

Never create typedefs or using aliases involving Unreal Engine template
types merely to make generated code appear complete.

For example, never generate:
typedef TSubclassOf<AActor> FActorSubclassOf;
using FActorSubclassOf = TSubclassOf<AActor>;

Only use an Unreal template type directly where it is required by an
actual declaration from the supplied project/engine context.

UNREAL TYPE USAGE / DEPENDENCY INTEGRITY
========================================
Never create declarations solely to make a type available.

If generated code uses an Unreal Engine type:
- Use the actual Unreal Engine type name.
- Use its real declaration from the supplied project/UE context.
- Include the appropriate real header when required.
- Forward-declare only ordinary C++ class/struct types when legal and actually
  needed.

Never create an alias, typedef, using declaration, wrapper, replacement type,
or substitute name for an existing Unreal Engine type.

FORBIDDEN:
- typedef TSubclassOf<class AActor> UClass;
- typedef TSubclassOf<class APlayerController> APlayerController;
- typedef TSubclassOf<class APlayerState> APlayerState;
- typedef TSubclassOf<class AGameStateBase> AGameStateBase;
- typedef TSubclassOf<class UGameInstance> UGameInstance;
- using AActor = ...;
- using APlayerController = ...;
- using UClass = ...;
- using TSubclassOf = ...;
- any declaration that attempts to redefine or rename an existing Unreal type.

Do not generate TSubclassOf declarations unless TSubclassOf is actually required
as a member/function/property type.

For example, this is valid when actually required:
TSubclassOf<AActor> ActorClass;

This is invalid:
template<typename T>
struct TSubclassOf;

typedef TSubclassOf<AActor> UClass;

Never generate a dependency declaration that is not directly required by an
actual member, parameter, return type, inheritance relationship, property,
function implementation, or other generated statement.

If the implementation does not use a type, do not declare it.

TSubclassOf is a normal Unreal Engine type. It does not require a manual
template declaration.

Never generate:
template<typename T>
struct TSubclassOf;

Never generate a TSubclassOf declaration unless the generated code actually
contains a TSubclassOf property, parameter, return type, or local variable.

If no generated code uses TSubclassOf, the string "TSubclassOf" must not appear
in the generated file.

)raw");

Prompt += TEXT(R"raw(

DEPENDENCIES
============
A project type may only be referenced if it:
1. already exists in supplied project context,
2. is explicitly being created by this request, or
3. is verified by supplied Unreal/plugin source.

DEPENDENCY DECLARATION RULE
===========================
Only declare a type when the generated code actually requires that type.

For every class/type referenced by generated code:
- Prefer the real existing declaration from the supplied project/UE source.
- Include the real header when required.
- A forward declaration is allowed only for a normal class/struct type when
  Unreal/C++ permits it and the complete definition is not required.
- Never create a forward declaration merely because the model expects a type.
- Never create a typedef, using declaration, alias, wrapper, or replacement type
  for an existing Unreal Engine or project type.
- Never redefine an existing Unreal Engine type.
- Never assign an Unreal Engine type to a different identifier.

Especially forbidden:
- typedef TSubclassOf<...> ...
- using ... = TSubclassOf<...>;
- struct/class declarations for TSubclassOf, TArray, TMap, TSet,
  TObjectPtr, TWeakObjectPtr, or other Unreal templates.
- typedefs that rename Unreal classes such as AActor, APlayerController,
  APlayerState, AGameStateBase, UClass, UGameInstance, etc.

If a type is not needed by the implementation, do not mention or declare it.

If a required type cannot be verified from supplied context, STOP and report the
missing dependency instead of inventing a declaration.

API EXPORT MACRO
================
Use the exact project API macro supplied by LocalStudio.
Never invent one from a class name.
If none is supplied, omit it.

)raw");

    Prompt += TEXT(R"raw(

DEPENDENCY MINIMALISM
=====================
When creating a new class from a simple request, do not add optional Unreal
types, components, controllers, gameplay systems, overrides, or helper
declarations unless the requested functionality requires them.

A class inheritance request means ONLY create the requested class and establish
the requested inheritance relationship.

Do not automatically add:
- Tick overrides
- BeginPlay overrides
- SetupPlayerInputComponent overrides
- PlayerController declarations
- CharacterMovementComponent declarations
- components
- gameplay systems
- replication
- timers
- delegates
- Blueprint functions
- getters/setters
- helper functions

unless they are required by the request or already required by the verified
parent-class contract.

Do not create declarations for types that are not referenced by the resulting
implementation.

)raw");

    Prompt += TEXT(R"raw(

MINIMAL IMPLEMENTATION
======================
Implement the smallest complete solution that satisfies the request.

When creating a class, the class name alone does not authorize additional
gameplay functionality.

Do not infer functionality from names such as:
- Survival
- Character
- Player
- Enemy
- Weapon
- Inventory
- Ability
- Component
- Manager
- GameMode
- GameState

Only implement functionality explicitly requested by the user or required
to satisfy an explicitly requested relationship/interface.

For a request such as:
"Create ASurvivalCharacter deriving from AMasterCharacter"

the expected result is ONLY a valid ASurvivalCharacter derived from
AMasterCharacter. Do not invent health, stamina, hunger, thirst, death,
respawn, inventory, combat, abilities, networking, camera, movement, or
other gameplay systems unless explicitly requested or already required
by the supplied class contract.

Do not:
- add speculative systems;
- enumerate unused Unreal overrides;
- add unused properties or functions;
- create placeholder managers/helpers/subsystems;
- build unrelated gameplay systems;
- expand a requested feature into a framework.

Only create systems explicitly requested or required by the implementation.
When creating a new class, do not add gameplay components, systems,
properties, functions, or behavior unless explicitly requested or required
by the class definition itself.

Do not proactively scaffold dependencies for future features.
Do not add "common" Unreal types simply because they may be useful later.

Do not infer likely future requirements from the class name.
For example, creating a SurvivalCharacter does not by itself authorize
adding health, inventory, stats, combat, camera, movement, or other systems.

When the requested functionality is complete, STOP.

RUNAWAY GENERATION
==================
Never generate repetitive, recursive, combinatorial, or mechanically expanded
identifiers/functions.

Do not create large families of functions by repeatedly combining subsystem
names such as Stats, Effects, UI, Animation, Physics, Collision, Visibility,
Replication, Network, Input, Logic, Behavior, or AI.

Every generated declaration must have a concrete purpose.
If generation becomes repetitive, STOP and return the smallest complete solution.

)raw");



    Prompt += TEXT(R"raw(

MULTIPLAYER / REPLICATION
=========================
All generated C++ should remain compatible with future multiplayer support,
but do not invent unnecessary networking systems.

For replicated gameplay state:

REPLICATION IS AN ALL-OR-NOTHING IMPLEMENTATION.
If a class contains even ONE UPROPERTY marked Replicated or ReplicatedUsing,
the complete replication implementation is mandatory.

When adding or modifying ANY replicated property in an Actor-derived class:

HEADER MUST CONTAIN:
- The UPROPERTY(Replicated) or UPROPERTY(ReplicatedUsing=...) declaration.
- A declaration for:
  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty>& OutLifetimeProps
  ) const override;

SOURCE MUST CONTAIN:
- #include "Net/UnrealNetwork.h" when required.
- The matching GetLifetimeReplicatedProps implementation.
- Super::GetLifetimeReplicatedProps(OutLifetimeProps);
- A DOREPLIFETIME or appropriate replication macro for EVERY replicated
  property.

Example:

Header:
UPROPERTY(Replicated)
int32 XP;

protected:
virtual void GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const override;

Source:
void AMyActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMyActor, XP);
}

NEVER generate UPROPERTY(Replicated) without also generating the complete
GetLifetimeReplicatedProps implementation.

NEVER generate GetLifetimeReplicatedProps without registering every
UPROPERTY(Replicated) member.

Before returning the JSON, count every Replicated/ReplicatedUsing property
and verify that each one has a corresponding replication registration.

If no replicated properties exist, do not add GetLifetimeReplicatedProps,
DOREPLIFETIME, or Net/UnrealNetwork.h unnecessarily.

Do not use GetOwnerHasAuthority() on Actors.
Do not substitute HasAuthority() with another remembered API.

SERVER AUTHORITY FOR REPLICATED STATE:
- Replicated gameplay state must be changed by the server.
- When a function directly modifies replicated gameplay state, verify that
  the modification occurs on the authoritative server.
- Use HasAuthority() when the function can be called from either server or
  client and no existing RPC/server pathway is supplied.
- Do not invent RPCs merely to satisfy this rule.
- Preserve an existing project RPC/server-authority architecture when one
  is supplied in the project context.

)raw");

Prompt += TEXT(R"raw(

GameMode:
- Exists only on the server.
- Never replicate GameMode properties.
- Never add GetLifetimeReplicatedProps to GameMode.
- Never add bReplicates to GameMode.
- Never use DOREPLIFETIME for a GameMode.
- Never create replicated player arrays, replicated timers, replicated match state,
  or other replicated properties in GameMode.
- Shared replicated match state belongs in GameState.
- Player-specific replicated state belongs in PlayerState.

GAME MODE MINIMALITY:
- When the user asks to create a GameMode, create ONLY the GameMode functionality
  explicitly requested.
- Do not automatically add Tick, PostLogin, Logout, custom player spawning,
  player tracking, game timers, GameState systems, or networking systems.
- If the request is to set a default character/pawn, use the existing project
  character class and set DefaultPawnClass.
- Do not create a TSubclassOf property merely to configure DefaultPawnClass.
- Do not create a custom pawn spawning function when DefaultPawnClass already
  satisfies the request.
- Do not create a custom GameState unless the user explicitly requests one.
- Do not create a custom PlayerState or PlayerController unless explicitly required.
- Multiplayer-ready does not mean adding multiplayer infrastructure.

Multiplayer-ready does NOT mean every class must contain replication code.
Only add networking/replication that the requested feature actually requires.

RPCs:
- Follow the exact supplied UE 5.7/project RPC pattern.
- Never invent _Validate functions.
- Only generate validation code when local UE/project context requires it.

)raw");

    Prompt += TEXT(R"raw(

CODE GENERATION INTEGRITY
==========================
Generate each declaration, function, member, include, and statement only once.

Never repeat the same declaration or function prototype.

Never enter a repetitive generation pattern.

Before returning the final code:
- Remove duplicate declarations.
- Remove duplicate functions.
- Remove duplicate includes.
- Remove repeated blocks of code.
- Ensure every function/member appears exactly once unless intentional overloads exist.
- Do not generate placeholder or repetitive declarations to fill output space.

If the requested implementation is large, produce the complete implementation once.
Do not repeat sections of the file.

)raw");

    Prompt += TEXT(R"raw(

BLUEPRINT PROPERTY EXPOSURE
===========================
When creating or modifying gameplay variables that are intended to be
available to Blueprints:

- Use UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "...") for
  variables that should be editable in the Blueprint Details panel and
  readable/writable from Blueprint graphs.
- Use UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...") when
  the variable should be editable in Details but only readable from
  Blueprint graphs.
- Use BlueprintReadOnly when Blueprint code should only read the property.
- Do not add Blueprint exposure to internal implementation variables unless
  the request requires Blueprint access.
- Preserve existing UPROPERTY specifiers that are unrelated to the request.
- Never create a duplicate Blueprint variable when the required property
  already exists in the C++ class.
- If an existing C++ property needs Blueprint exposure, modify its existing
  UPROPERTY declaration rather than creating a new property.

When a new gameplay variable is explicitly requested without specifying
its Blueprint visibility, prefer:
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AppropriateCategory")

Do not apply EditAnywhere or BlueprintReadWrite to replication metadata,
internal timers, handles, cached references, or other implementation-only
members unless explicitly requested.

FUNCTIONS
---------
Gameplay functions exposed to Blueprint:
UFUNCTION(BlueprintCallable, Category = "...")

PROPERTIES
----------
Gameplay properties intended for designer configuration:
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "...")

BLUEPRINT COMPATIBILITY
=======================

C++ classes intended to serve as parent classes for Unreal Engine
Blueprints MUST be declared with:

UCLASS(Blueprintable, BlueprintType)

When creating or modifying a C++ class that is intended to be
Blueprint-derived or explicitly requested to be usable as a Blueprint
parent, ensure the UCLASS declaration contains Blueprintable.

BlueprintType should also be included when the class should be usable
as a Blueprint variable type or Blueprint pin type.

Do not remove existing Blueprintable or BlueprintType specifiers from
a class.

When a class is explicitly described as a Blueprint parent, Blueprint
base class, or intended to have Blueprint children, use:

UCLASS(Blueprintable, BlueprintType)

)raw");

    Prompt += TEXT(R"raw(

BUILD.CS
========
Do not modify Build.cs through this generation operation.
If additional modules appear necessary, report them in the JSON summary.

DELEGATES / BLUEPRINT EVENTS
============================
UPROPERTY(BlueprintAssignable) MUST use an appropriate dynamic multicast
delegate declared with DECLARE_DYNAMIC_MULTICAST_DELEGATE*.

Never use FScriptDelegate or a single-cast delegate for BlueprintAssignable.

Do not fix BlueprintAssignable errors by adding random includes or changing
visibility.

FINAL SELF-CHECK
================
Before returning JSON verify:

1. Requested change only; unrelated code preserved.
2. Header/source class names and declarations match.
3. All required includes are present and *.generated.h is last.
4. No invented project or Unreal APIs/types exist.
5. No invalid template forward declarations, aliases, or malformed declarations.
6. Replication follows the supplied UE 5.7 pattern.
7. No unnecessary declarations or speculative systems exist.
8. Output is complete, coherent, non-repetitive, and not truncated.
9. For every UPROPERTY marked Replicated or ReplicatedUsing:
   - GetLifetimeReplicatedProps is declared in the header.
   - GetLifetimeReplicatedProps is implemented exactly once in the source.
   - Super::GetLifetimeReplicatedProps is called.
   - Every replicated property is registered with the appropriate
     DOREPLIFETIME macro.
   - Required replication includes are present.
   - No replication code exists when there are no replicated properties.

If the full requested implementation cannot fit in the output budget,
prefer a smaller complete implementation over speculative or repetitive code.

The supplied project source, local Unreal Engine 5.7 source, and compiler/UHT
errors always take precedence over assumptions.
)raw");

    return Prompt;
}

FString FLocalStudioCodeBuilder::GetTechnicalGuardrails() const
{
    FString ProjectName = FApp::GetProjectName();
    FString CompleteGuardrails = GetMinimalSystemPrompt();

    CompleteGuardrails += FString::Printf(TEXT("\nReturn valid JSON format matching this exact schema:\n")
        TEXT("{\n")
        TEXT("  \"action\": \"create_or_modify\" | \"delete\",\n")
        TEXT("  \"summary\": \"Description of code additions, modifications, optimizations, or deletions\",\n")
        TEXT("  \"header_path\": \"Source/%s/Public/[SubFolder]/Filename.h\",\n")
        TEXT("  \"header_content\": \"FULL un-truncated header file content here (leave empty if deleting or cpp-only)\",\n")
        TEXT("  \"source_path\": \"Source/%s/Private/[SubFolder]/Filename.cpp\",\n")
        TEXT("  \"source_content\": \"FULL un-truncated source file content here (leave empty if deleting or header-only)\"\n")
        TEXT("}"),
        *ProjectName, *ProjectName
    );

    return CompleteGuardrails;
}

FString FLocalStudioCodeBuilder::PreparePromptContext(const FString& RawUserPrompt)
{
    FString SourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
    FString ProjectModuleName = FApp::GetProjectName();
    FString ProjectApiMacro = ProjectModuleName.ToUpper() + TEXT("_API");

    FString Context;

    Context += TEXT("============================================================\n");
    Context += TEXT("LOCALSTUDIO AUTHORITATIVE PROJECT CONTEXT\n");
    Context += TEXT("============================================================\n\n");

    Context += TEXT("ENGINE VERSION:\n");
    Context += TEXT("Unreal Engine 5.7\n\n");

    Context += TEXT("============================================================\n");
    Context += TEXT("PROJECT MODULE INFORMATION\n");
    Context += TEXT("============================================================\n");
    Context += FString::Printf(
        TEXT("Project module name: %s\n"),
        *ProjectModuleName
    );
    Context += FString::Printf(
        TEXT("Project API export macro: %s\n"),
        *ProjectApiMacro
    );
    Context += TEXT("\n");

    // ------------------------------------------------------------
    // 1. Identify project source files
    // ------------------------------------------------------------

    TArray<FString> ProjectHeaders;
    TArray<FString> ProjectSources;

    IFileManager::Get().FindFilesRecursive(
        ProjectHeaders,
        *SourceDir,
        TEXT("*.h"),
        true,
        false,
        false
    );

    IFileManager::Get().FindFilesRecursive(
        ProjectSources,
        *SourceDir,
        TEXT("*.cpp"),
        true,
        false,
        false
    );

    // ------------------------------------------------------------
    // 2. Find project files that are likely relevant
    // ------------------------------------------------------------

    TSet<FString> RelevantProjectFiles;

    auto AddRelevantFile = [&](const FString& FilePath)
        {
            if (!FilePath.IsEmpty())
            {
                RelevantProjectFiles.Add(FilePath);
            }
        };

    for (const FString& Header : ProjectHeaders)
    {
        FString FileName = FPaths::GetCleanFilename(Header);
        FString BaseName = FPaths::GetBaseFilename(Header);

        if (RawUserPrompt.Contains(FileName, ESearchCase::IgnoreCase) ||
            RawUserPrompt.Contains(BaseName, ESearchCase::IgnoreCase))
        {
            AddRelevantFile(Header);

            // Find corresponding CPP.
            for (const FString& Source : ProjectSources)
            {
                if (FPaths::GetBaseFilename(Source).Equals(BaseName, ESearchCase::IgnoreCase))
                {
                    AddRelevantFile(Source);
                }
            }
        }
    }

    // ------------------------------------------------------------
    // 3. Extract obvious class names from the request
    // ------------------------------------------------------------

    TArray<FString> ImportantTokens;
    RawUserPrompt.ParseIntoArrayWS(ImportantTokens);

    for (const FString& Token : ImportantTokens)
    {
        FString CleanToken = Token;

        CleanToken.ReplaceInline(TEXT("\""), TEXT(""));
        CleanToken.ReplaceInline(TEXT("'"), TEXT(""));
        CleanToken.ReplaceInline(TEXT(","), TEXT(""));
        CleanToken.ReplaceInline(TEXT("."), TEXT(""));
        CleanToken.ReplaceInline(TEXT("("), TEXT(""));
        CleanToken.ReplaceInline(TEXT(")"), TEXT(""));
        CleanToken.ReplaceInline(TEXT(";"), TEXT(""));

        if (CleanToken.Len() < 4)
        {
            continue;
        }

        for (const FString& Header : ProjectHeaders)
        {
            FString BaseName = FPaths::GetBaseFilename(Header);

            if (BaseName.Equals(CleanToken, ESearchCase::IgnoreCase) ||
                BaseName.Contains(CleanToken, ESearchCase::IgnoreCase) ||
                CleanToken.Contains(BaseName, ESearchCase::IgnoreCase))
            {
                AddRelevantFile(Header);

                for (const FString& Source : ProjectSources)
                {
                    if (FPaths::GetBaseFilename(Source).Equals(BaseName, ESearchCase::IgnoreCase))
                    {
                        AddRelevantFile(Source);
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------
    // 4. If we found no obvious target, inspect project source
    //    intelligently rather than dumping everything.
    // ------------------------------------------------------------

    if (RelevantProjectFiles.Num() == 0)
    {
        // Prefer files whose names contain important UE concepts.
        const TArray<FString> ImportantWords =
        {
            TEXT("GameMode"),
            TEXT("GameState"),
            TEXT("PlayerState"),
            TEXT("PlayerController"),
            TEXT("Character"),
            TEXT("Pawn"),
            TEXT("Component"),
            TEXT("Subsystem"),
            TEXT("Widget"),
            TEXT("Actor")
        };

        for (const FString& Header : ProjectHeaders)
        {
            FString BaseName = FPaths::GetBaseFilename(Header);

            for (const FString& Word : ImportantWords)
            {
                if (BaseName.Contains(Word, ESearchCase::IgnoreCase))
                {
                    AddRelevantFile(Header);

                    for (const FString& Source : ProjectSources)
                    {
                        if (FPaths::GetBaseFilename(Source).Equals(BaseName, ESearchCase::IgnoreCase))
                        {
                            AddRelevantFile(Source);
                        }
                    }

                    break;
                }
            }

            if (RelevantProjectFiles.Num() >= 8)
            {
                break;
            }
        }
    }

    // ------------------------------------------------------------
    // 5. Emit project context
    // ------------------------------------------------------------

    Context += TEXT("=== EXISTING PROJECT FILES ===\n");

    int32 ProjectFileCount = 0;

    for (const FString& FilePath : RelevantProjectFiles)
    {
        if (ProjectFileCount >= 12)
        {
            break;
        }

        FString RelativePath = FilePath;
        FPaths::MakePathRelativeTo(RelativePath, *FPaths::ProjectDir());

        FString FileContent = ReadSourceFile(FilePath);

        if (FileContent.IsEmpty())
        {
            continue;
        }

        Context += FString::Printf(
            TEXT("\n--- PROJECT FILE: %s ---\n%s\n--- END PROJECT FILE ---\n"),
            *RelativePath,
            *FileContent
        );

        ProjectFileCount++;
    }

    if (ProjectFileCount == 0)
    {
        Context += TEXT("No existing project files were confidently identified.\n");
    }

    // ------------------------------------------------------------
    // 6. Determine likely Unreal classes/types mentioned
    // ------------------------------------------------------------

    TArray<FString> UnrealSymbols =
    {
        TEXT("DOREPLIFETIME"),
        TEXT("GetLifetimeReplicatedProps"),
        TEXT("FLifetimeProperty"),
        TEXT("FComponentReplicationInfo"),
        TEXT("HasAuthority"),
        TEXT("SetReplicates"),
        TEXT("SetIsReplicatedByDefault"),
        TEXT("AActor"),
        TEXT("APawn"),
        TEXT("ACharacter"),
        TEXT("APlayerController"),
        TEXT("APlayerState"),
        TEXT("AGameModeBase"),
        TEXT("AGameMode"),
        TEXT("AGameStateBase"),
        TEXT("AGameState"),
        TEXT("UActorComponent"),
        TEXT("USceneComponent"),
        TEXT("UUserWidget"),
        TEXT("UGameInstance"),
        TEXT("UWorld"),
        TEXT("UObject"),
        TEXT("FVector"),
        TEXT("FRotator"),
        TEXT("FTransform"),
        TEXT("FHitResult"),
        TEXT("FInputActionValue"),
        TEXT("TArray"),
        TEXT("TMap"),
        TEXT("TSet"),
        TEXT("TSubclassOf"),
        TEXT("UFUNCTION"),
        TEXT("UPROPERTY"),
        TEXT("USTRUCT"),
        TEXT("UENUM"),
        TEXT("DOREPLIFETIME"),
        TEXT("GetLifetimeReplicatedProps"),
        TEXT("HasAuthority"),
        TEXT("GameplayStatics"),
        TEXT("EnhancedInput"),
        TEXT("UEnhancedInputComponent"),
        TEXT("UInputAction"),
        TEXT("UInputMappingContext"),
        TEXT("UWidget"),
        TEXT("UUserWidget"),
        TEXT("TActorIterator")
    };

    TArray<FString> RelevantSymbols;

    for (const FString& Symbol : UnrealSymbols)
    {
        if (RawUserPrompt.Contains(Symbol, ESearchCase::IgnoreCase))
        {
            RelevantSymbols.Add(Symbol);
        }
    }

    // Also inspect the project context for symbols the user did not explicitly name.
    for (const FString& FilePath : RelevantProjectFiles)
    {
        FString FileContent = ReadSourceFile(FilePath);

        for (const FString& Symbol : UnrealSymbols)
        {
            if (FileContent.Contains(Symbol, ESearchCase::IgnoreCase))
            {
                RelevantSymbols.AddUnique(Symbol);
            }
        }
    }

    // ------------------------------------------------------------
    // 7. Unreal Engine source retrieval
    //
    // This is the local "training" layer.
    // We do NOT modify the model.
    // We teach it from the installed UE source every request.
    // ------------------------------------------------------------

    Context += TEXT("\n=== AUTHORITATIVE UNREAL ENGINE 5.7 SOURCE ===\n");

    int32 EngineContextCount = 0;

    for (const FString& Symbol : RelevantSymbols)
    {
        if (EngineContextCount >= 10)
        {
            break;
        }

        FString EngineContext = FindEngineHeader(Symbol);

        if (!EngineContext.IsEmpty())
        {
            Context += FString::Printf(
                TEXT("\n--- UE5.7 SOURCE REFERENCE: %s ---\n%s\n--- END UE5.7 SOURCE REFERENCE ---\n"),
                *Symbol,
                *EngineContext
            );

            EngineContextCount++;
        }
    }

    if (EngineContextCount == 0)
    {
        Context += TEXT("No specific engine source symbol was confidently identified.\n");
    }

    // ------------------------------------------------------------
    // 8. Runtime reflection information
    // ------------------------------------------------------------

    FString ReflectionData;

    TArray<FString> ParentCandidates =
    {
        TEXT("Actor"),
        TEXT("Pawn"),
        TEXT("Character"),
        TEXT("PlayerController"),
        TEXT("PlayerState"),
        TEXT("GameModeBase"),
        TEXT("GameMode"),
        TEXT("GameStateBase"),
        TEXT("GameState"),
        TEXT("ActorComponent"),
        TEXT("SceneComponent"),
        TEXT("UserWidget"),
        TEXT("GameInstance")
    };

    for (const FString& Parent : ParentCandidates)
    {
        if (RawUserPrompt.Contains(Parent, ESearchCase::IgnoreCase))
        {
            ReflectionData += BuildParentClassReflectionContext(Parent);
            ReflectionData += TEXT("\n");
        }
    }

    if (!ReflectionData.IsEmpty())
    {
        Context += TEXT("\n=== UNREAL RUNTIME REFLECTION ===\n");
        Context += ReflectionData;
    }

    // ------------------------------------------------------------
    // 9. Explicit current request
    // ------------------------------------------------------------

    Context += TEXT("\n============================================================\n");
    Context += TEXT("USER REQUEST\n");
    Context += TEXT("============================================================\n");
    Context += RawUserPrompt;
    Context += TEXT("\n");

    Context += TEXT("\n============================================================\n");
    Context += TEXT("CONTEXT AUTHORITY ORDER\n");
    Context += TEXT("============================================================\n");
    Context += TEXT("1. Existing project source files\n");
    Context += TEXT("2. Installed Unreal Engine 5.7 source\n");
    Context += TEXT("3. Unreal runtime reflection information\n");
    Context += TEXT("4. Compiler/build information\n");
    Context += TEXT("5. Model's prior knowledge\n");

    return Context;
}

// -----------------------------------------------------------------------------------
// Execution Logic
// -----------------------------------------------------------------------------------

bool FLocalStudioCodeBuilder::ParseResponse(const FString& RawResponse, TSharedPtr<FJsonObject>& OutParsedData, FString& OutError)
{
    return FLocalStudioJsonUtils::ExtractAndParseJsonObject(RawResponse, OutParsedData, OutError);
}

FBuilderExecutionResult FLocalStudioCodeBuilder::ExecutePlan(TSharedPtr<FJsonObject> ParsedData)
{
    FBuilderExecutionResult Result;
    Result.bSuccess = false;

    if (!ParsedData.IsValid())
    {
        Result.UserSummary = TEXT("Invalid C++ Code Generation payload.");
        return Result;
    }

    FString Action = ParsedData->HasField(TEXT("action"))
        ? ParsedData->GetStringField(TEXT("action"))
        : TEXT("create_or_modify");

    FString HeaderPath = ParsedData->HasField(TEXT("header_path"))
        ? ParsedData->GetStringField(TEXT("header_path"))
        : TEXT("");

    FString HeaderContent = ParsedData->HasField(TEXT("header_content"))
        ? ParsedData->GetStringField(TEXT("header_content"))
        : TEXT("");

    FString SourcePath = ParsedData->HasField(TEXT("source_path"))
        ? ParsedData->GetStringField(TEXT("source_path"))
        : TEXT("");

    FString SourceContent = ParsedData->HasField(TEXT("source_content"))
        ? ParsedData->GetStringField(TEXT("source_content"))
        : TEXT("");

    FString Summary = ParsedData->HasField(TEXT("summary"))
        ? ParsedData->GetStringField(TEXT("summary"))
        : TEXT("");

    UE_LOG(
        LogTemp,
        Log,
        TEXT("LocalStudio ExecutePlan: Action=[%s] Header=[%s] Source=[%s]"),
        *Action,
        *HeaderPath,
        *SourcePath
    );

    // ============================================================
    // DELETE
    // ============================================================

    if (Action.Equals(TEXT("delete"), ESearchCase::IgnoreCase))
    {
        FString FileError;

        bool bDeletedHeader = HeaderPath.IsEmpty()
            ? true
            : DeleteSourceFile(HeaderPath, FileError);

        bool bDeletedSource = SourcePath.IsEmpty()
            ? true
            : DeleteSourceFile(SourcePath, FileError);

        if (!bDeletedHeader || !bDeletedSource)
        {
            Result.UserSummary = FString::Printf(
                TEXT("C++ deletion failed: %s"),
                *FileError
            );

            return Result;
        }

        Result.bSuccess = true;

        Result.UserSummary = FString::Printf(
            TEXT("%s\n\nRequested C++ files deleted."),
            *Summary
        );

        return Result;
    }

    // ============================================================
    // NO SANITIZER
    //
    // IMPORTANT:
    // The LLM output is now treated as the proposed source.
    //
    // We do NOT perform string-based "healing".
    // ============================================================

    UE_LOG(
        LogTemp,
        Log,
        TEXT("LocalStudio: CodeBuilder received generated source without automatic code rewriting.")
    );

    // ============================================================
    // BASIC STRUCTURAL VALIDATION
    //
    // These checks reject obviously unsafe output before touching
    // the user's source files.
    // ============================================================

    UE_LOG(
        LogTemp,
        Log,
        TEXT("LocalStudio ExecutePlan: Validating generated files.")
    );

    auto ValidateGeneratedFile =
        [](const FString& Path, const FString& Content, bool bIsHeader, FString& OutError) -> bool
        {
            if (Path.IsEmpty())
            {
                return true;
            }

            if (Content.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("Error: Generated file [%s] was empty. The model failed to produce code."),
                    *Path
                );
                return false;
            }

            if (!Path.StartsWith(TEXT("Source/")) && !Path.StartsWith(TEXT("Plugins/")))
            {
                OutError = FString::Printf(
                    TEXT("Path Error: Refusing to write to '%s'. Files must remain inside Source/ or Plugins/."),
                    *Path
                );
                return false;
            }

            if (Path.Contains(TEXT("..")))
            {
                OutError = FString::Printf(TEXT("Path Error: Relative path traversal detected in '%s'."), *Path);
                return false;
            }

            if (Path.EndsWith(TEXT(".Build.cs")))
            {
                OutError = TEXT("Build Error: Direct modifications to .Build.cs are not allowed via code generation.");
                return false;
            }

            if (Content.Contains(TEXT("```")))
            {
                OutError = FString::Printf(TEXT("Formatting Error: Response contained raw Markdown fences in '%s'."), *Path);
                return false;
            }

            // Header-specific structural checks.
            if (bIsHeader)
            {
                int32 GeneratedIndex = Content.Find(TEXT(".generated.h"));

                // Rule: Check for invalid GameMode Replication
                if (Path.Contains(TEXT("GameMode")))
                {
                    if (Content.Contains(TEXT("UPROPERTY(Replicated")) || Content.Contains(TEXT("GetLifetimeReplicatedProps")))
                    {
                        OutError = FString::Printf(
                            TEXT("Unreal Architecture Error in '%s':\n")
                            TEXT("GameMode exists ONLY on the server and does NOT support replication.\n")
                            TEXT("Replicated properties (like player counts or timers) must be placed in a GameState class instead."),
                            *Path
                        );

                        UE_LOG(
                            LogTemp,
                            Error,
                            TEXT("LocalStudio ExecutePlan validation failed:\n%s"),
                            *OutError
                        );

                        return false;
                    }
                }

                // Rule: Ensure .generated.h is last include
                if (GeneratedIndex != INDEX_NONE)
                {
                    int32 LastInclude = INDEX_NONE;
                    int32 SearchPos = 0;

                    while (true)
                    {
                        int32 IncludeIndex = Content.Find(
                            TEXT("#include"),
                            ESearchCase::IgnoreCase,
                            ESearchDir::FromStart,
                            SearchPos
                        );

                        if (IncludeIndex == INDEX_NONE)
                        {
                            break;
                        }

                        LastInclude = IncludeIndex;
                        SearchPos = IncludeIndex + 8;
                    }

                    if (LastInclude != INDEX_NONE && GeneratedIndex < LastInclude)
                    {
                        OutError = FString::Printf(
                            TEXT("Header Error in '%s': '#include \"*.generated.h\"' must be the absolute last include in the file."),
                            *Path
                        );
                        return false;
                    }
                }

                // Reject obvious invalid template forward declarations.
                if (Content.Contains(TEXT("template<typename T> class TArray")) ||
                    Content.Contains(TEXT("template<typename T> struct TArray")) ||
                    Content.Contains(TEXT("template<typename T> class TMap")) ||
                    Content.Contains(TEXT("template<typename T> struct TMap")) ||
                    Content.Contains(TEXT("template<typename T> class TSubclassOf")) ||
                    Content.Contains(TEXT("template<typename T> struct TSubclassOf")))
                {
                    OutError = FString::Printf(
                        TEXT("Invalid Unreal template forward declaration detected in: %s"),
                        *Path
                    );

                    return false;
                }

                // ============================================================
                // INVALID UNREAL TEMPLATE FORWARD DECLARATIONS
                // ============================================================
                //
                // Unreal Engine container/template types such as TArray, TMap,
                // TSet, TSubclassOf, TSoftObjectPtr, etc. must not be manually
                // forward declared by generated project code.
                //
                // In particular, reject malformed attempts such as:
                //
                // template<typename T>
                // struct TSubclassOf;
                //
                // typename TSubclassOf<class AActor>;
                //
                // These declarations are invalid C++ and/or conflict with the
                // actual Unreal Engine template definitions.
                //
                // The model must include the appropriate Unreal header instead.
                // ============================================================

                const TArray<FString> InvalidTemplateNames =
                {
                    TEXT("TArray"),
                    TEXT("TMap"),
                    TEXT("TSet"),
                    TEXT("TSubclassOf"),
                    TEXT("TSoftObjectPtr"),
                    TEXT("TSoftClassPtr"),
                    TEXT("TWeakObjectPtr"),
                    TEXT("TStrongObjectPtr"),
                    TEXT("TObjectPtr")
                };

                for (const FString& TemplateName : InvalidTemplateNames)
                {
                    const FString TemplateForwardDeclaration =
                        FString::Printf(
                            TEXT("template<typename T>\\nstruct %s"),
                            *TemplateName
                        );

                    const FString TemplateForwardDeclarationClass =
                        FString::Printf(
                            TEXT("template<typename T>\\nclass %s"),
                            *TemplateName
                        );

                    if (Content.Contains(TemplateForwardDeclaration) ||
                        Content.Contains(TemplateForwardDeclarationClass))
                    {
                        OutError = FString::Printf(
                            TEXT("Invalid Unreal template forward declaration detected in '%s': %s.\n\n")
                            TEXT("Do not manually forward declare Unreal Engine template types such as %s.\n")
                            TEXT("Use the appropriate Unreal Engine include instead."),
                            *Path,
                            *TemplateName,
                            *TemplateName
                        );

                        return false;
                    }
                }

                // Reject malformed 'typename TSubclassOf<...>' declarations.
                if (Content.Contains(TEXT("typename TSubclassOf<")))
                {
                    OutError = FString::Printf(
                        TEXT("Invalid C++ declaration detected in '%s':\n")
                        TEXT("'typename TSubclassOf<...>' is not a valid standalone declaration.\n")
                        TEXT("Do not manually declare or instantiate TSubclassOf this way."),
                        *Path
                    );

                    return false;
                }

                // Rule: Check missing GetLifetimeReplicatedProps
                if (Content.Contains(TEXT("Replicated")) || Content.Contains(TEXT("ReplicatedUsing")))
                {
                    if (!Content.Contains(TEXT("GetLifetimeReplicatedProps")))
                    {
                        OutError = FString::Printf(
                            TEXT("Replication Error in '%s':\n")
                            TEXT("Class declares UPROPERTY(Replicated) but is missing 'virtual void GetLifetimeReplicatedProps(...) const override;'."),
                            *Path
                        );
                        return false;
                    }
                }
            }

            return true;
        };

    FString ValidationError;

    if (!ValidateGeneratedFile(
        HeaderPath,
        HeaderContent,
        true,
        ValidationError))
    {
        Result.UserSummary = FString::Printf(
            TEXT("Generated header rejected before writing:\n%s"),
            *ValidationError
        );

        return Result;
    }

    if (!ValidateGeneratedFile(
        SourcePath,
        SourceContent,
        false,
        ValidationError))
    {
        Result.UserSummary = FString::Printf(
            TEXT("Generated source rejected before writing:\n%s"),
            *ValidationError
        );

        return Result;
    }

    // ============================================================
    // VERIFY HEADER / SOURCE PAIRING
    // ============================================================

    if (!HeaderPath.IsEmpty() && !SourcePath.IsEmpty())
    {
        FString HeaderBase = FPaths::GetBaseFilename(HeaderPath);
        FString SourceBase = FPaths::GetBaseFilename(SourcePath);

        if (!HeaderBase.Equals(SourceBase, ESearchCase::IgnoreCase))
        {
            Result.UserSummary = FString::Printf(
                TEXT("Header/source mismatch rejected.\nHeader: %s\nSource: %s"),
                *HeaderPath,
                *SourcePath
            );

            return Result;
        }
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("LocalStudio ExecutePlan: Validating generated files.")
    );

    // ============================================================
    // LIVE CODING SAFETY CHECK
    // ============================================================

    if (FModuleManager::Get().IsModuleLoaded(LIVE_CODING_MODULE_NAME))
    {
        ILiveCodingModule& LiveCoding =
            FModuleManager::GetModuleChecked<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);

        if (LiveCoding.IsCompiling())
        {
            Result.bSuccess = false;

            Result.UserSummary =
                TEXT("C++ modification delayed because Live Coding is currently compiling.\n\n")
                TEXT("Please wait for the current compilation to finish and try the modification again.");

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("LocalStudio: ExecutePlan rejected because Live Coding is currently compiling.")
            );

            return Result;
        }
    }

    // ============================================================
    // CREATE BACKUP DIRECTORY
    // ============================================================

    FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

    FString BackupRoot = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("LocalStudio"),
        TEXT("Backups"),
        Timestamp
    );

    if (!IFileManager::Get().MakeDirectory(*BackupRoot, true))
    {
        Result.UserSummary = FString::Printf(
            TEXT("Could not create LocalStudio backup directory:\n%s"),
            *BackupRoot
        );

        return Result;
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("LocalStudio ExecutePlan: Backup directory=[%s]"),
        *BackupRoot
    );

    // ============================================================
    // BACKUP EXISTING FILES
    // ============================================================

    

    auto BackupFile =
        [&](const FString& RelativePath, FString& OutError) -> bool
        {
            if (RelativePath.IsEmpty())
            {
                return true;
            }

            FString FullPath = FPaths::Combine(
                FPaths::ProjectDir(),
                RelativePath
            );

            if (!FPaths::FileExists(FullPath))
            {
                return true;
            }

            FString RelativeBackupPath = RelativePath;

            RelativeBackupPath.ReplaceInline(
                TEXT("/"),
                TEXT("_")
            );

            FString BackupPath = FPaths::Combine(
                BackupRoot,
                RelativeBackupPath
            );

            // Normalize both paths before attempting the copy.
            FPaths::NormalizeFilename(FullPath);
            FPaths::NormalizeFilename(BackupPath);

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio Backup: Source=[%s]"),
                *FullPath
            );

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio Backup: Destination=[%s]"),
                *BackupPath
            );

            // Verify the source actually exists.
            if (!IFileManager::Get().FileExists(*FullPath))
            {
                OutError = FString::Printf(
                    TEXT("Backup failed because the existing file does not exist:\n%s"),
                    *FullPath
                );

                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("LocalStudio Backup: SOURCE DOES NOT EXIST: %s"),
                    *FullPath
                );

                return false;
            }

            // Verify that Unreal can read the source file.
            FString TestContents;

            if (!FFileHelper::LoadFileToString(TestContents, *FullPath))
            {
                OutError = FString::Printf(
                    TEXT("Backup failed because Unreal could not read the existing file:\n%s"),
                    *FullPath
                );

                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("LocalStudio Backup: SOURCE COULD NOT BE READ: %s"),
                    *FullPath
                );

                return false;
            }

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio Backup: Source is readable. Size=%lld bytes"),
                static_cast<long long>(TestContents.Len())
            );

            // Verify the backup directory exists.
            if (!IFileManager::Get().DirectoryExists(*BackupRoot))
            {
                OutError = FString::Printf(
                    TEXT("Backup directory does not exist:\n%s"),
                    *BackupRoot
                );

                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("LocalStudio Backup: BACKUP DIRECTORY DOES NOT EXIST: %s"),
                    *BackupRoot
                );

                return false;
            }

            // Attempt the actual copy.
            const uint32 CopyResult = IFileManager::Get().Copy(
                *BackupPath,
                *FullPath,
                true,
                true
            );

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio Backup: CopyResult=%u"),
                CopyResult
            );

            if (CopyResult != COPY_OK)
            {
                OutError = FString::Printf(
                    TEXT("Could not backup existing file.\n\n")
                    TEXT("Source:\n%s\n\n")
                    TEXT("Destination:\n%s\n\n")
                    TEXT("CopyResult: %u"),
                    *FullPath,
                    *BackupPath,
                    CopyResult
                );

                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("LocalStudio Backup: COPY FAILED. CopyResult=%u"),
                    CopyResult
                );

                return false;
            }

            if (!IFileManager::Get().FileExists(*BackupPath))
            {
                OutError = FString::Printf(
                    TEXT("Backup operation reported success, but the backup file was not found:\n%s"),
                    *BackupPath
                );

                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("LocalStudio Backup: COPY REPORTED SUCCESS BUT BACKUP DOES NOT EXIST: %s"),
                    *BackupPath
                );

                return false;
            }

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio Backup: Successfully backed up [%s] to [%s]"),
                *FullPath,
                *BackupPath
            );

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio Backup: Successfully backed up [%s] to [%s]"),
                *FullPath,
                *BackupPath
            );

            UE_LOG(
                LogTemp,
                Log,
                TEXT("LocalStudio ExecutePlan: Checking existing file for backup=[%s]"),
                *FullPath
            );

            return true;
        };

    FString BackupError;

    if (!BackupFile(HeaderPath, BackupError) ||
        !BackupFile(SourcePath, BackupError))
    {
        Result.UserSummary = BackupError;
        return Result;
    }

    // ============================================================
    // WRITE GENERATED FILES
    // ============================================================

    FString FileError;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("LocalStudio ExecutePlan: Writing header=[%s]"),
        *HeaderPath
    );

    UE_LOG(
        LogTemp,
        Log,
        TEXT("LocalStudio ExecutePlan: Writing source=[%s]"),
        *SourcePath
    );

    if (!HeaderPath.IsEmpty() && !HeaderContent.IsEmpty())
    {
        if (!WriteSourceFile(HeaderPath, HeaderContent, FileError))
        {
            Result.UserSummary = FString::Printf(
                TEXT("Failed writing generated header:\n%s\n\nBackup retained at:\n%s"),
                *FileError,
                *BackupRoot
            );
            return Result;
        }
        EnsureBuildDependencies(HeaderContent); // Check header for required Build.cs modules
    }

    if (!SourcePath.IsEmpty() && !SourceContent.IsEmpty())
    {
        if (!WriteSourceFile(SourcePath, SourceContent, FileError))
        {
            Result.UserSummary = FString::Printf(
                TEXT("Failed writing generated source:\n%s\n\nBackup retained at:\n%s"),
                *FileError,
                *BackupRoot
            );
            return Result;
        }
        EnsureBuildDependencies(SourceContent); // Check source for required Build.cs modules
    }

    // ============================================================
    // REFRESH PROJECT FILES
    // ============================================================

    FString UnrealEngineDir =
        FPaths::ConvertRelativePathToFull(
            FGenericPlatformMisc::EngineDir()
        );

    FString ProjectPath =
        FPaths::ConvertRelativePathToFull(
            FPaths::GetProjectFilePath()
        );

#if PLATFORM_WINDOWS
    FString UBTPath = FPaths::Combine(
        UnrealEngineDir,
        TEXT("Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe")
    );
#else
    FString UBTPath = FPaths::Combine(
        UnrealEngineDir,
        TEXT("Binaries/DotNET/UnrealBuildTool/UnrealBuildTool")
    );
#endif

    if (FPaths::FileExists(UBTPath))
    {
        FString CmdArgs = FString::Printf(
            TEXT("-projectfiles -project=\"%s\" -game -engine"),
            *ProjectPath
        );

        FProcHandle Proc = FPlatformProcess::CreateProc(
            *UBTPath,
            *CmdArgs,
            true,
            false,
            false,
            nullptr,
            0,
            nullptr,
            nullptr
        );

        if (Proc.IsValid())
        {
            FPlatformProcess::WaitForProc(Proc);
            FPlatformProcess::CloseProc(Proc);
        }
    }

    // ============================================================
    // LIVE CODING
    // ============================================================

    FString CompileLog;
    TriggerLiveCompilation(CompileLog);

    Result.bSuccess = true;

    Result.UserSummary = FString::Printf(
        TEXT("%s\n\n")
        TEXT("C++ changes applied.\n\n")
        TEXT("IMPORTANT: Generated code was NOT automatically rewritten.\n")
        TEXT("The source sent by the local model is now the source being compiled.\n\n")
        TEXT("Backup:\n%s\n\n")
        TEXT("Live Coding:\n%s"),
        *Summary,
        *BackupRoot,
        *CompileLog
    );

    return Result;
}

bool FLocalStudioCodeBuilder::WriteSourceFile(const FString& RelativeFilePath, const FString& FileContent, FString& OutError)
{
    FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativeFilePath);
    FString CleanFileName = FPaths::GetCleanFilename(RelativeFilePath);
    TArray<FString> FoundFiles;
    IFileManager::Get().FindFilesRecursive(FoundFiles, *FPaths::GameSourceDir(), *CleanFileName, true, false);

    if (FoundFiles.Num() > 0)
    {
        FullPath = FoundFiles[0];
    }
    else
    {
        FString Directory = FPaths::GetPath(FullPath);
        if (!IFileManager::Get().DirectoryExists(*Directory))
        {
            IFileManager::Get().MakeDirectory(*Directory, true);
        }
    }

    // Convert Unix newlines to Windows standard CRLF without stripping empty lines
    FString NormalizedContent = FileContent;
    NormalizedContent.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    NormalizedContent.ReplaceInline(TEXT("\n"), TEXT("\r\n"));

    if (FFileHelper::SaveStringToFile(NormalizedContent, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogTemp, Log, TEXT("LocalStudio: Saved C++ file [%s]"), *FullPath);
        return true;
    }

    OutError = FString::Printf(TEXT("Could not write file to: %s"), *FullPath);
    return false;
}

bool FLocalStudioCodeBuilder::DeleteSourceFile(const FString& RelativeFilePath, FString& OutError)
{
    FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativeFilePath);
    FString CleanFileName = FPaths::GetCleanFilename(RelativeFilePath);
    TArray<FString> FoundFiles;
    IFileManager::Get().FindFilesRecursive(FoundFiles, *FPaths::GameSourceDir(), *CleanFileName, true, false);

    if (FoundFiles.Num() > 0)
    {
        FullPath = FoundFiles[0];
    }

    if (FPaths::FileExists(FullPath))
    {
        if (IFileManager::Get().Delete(*FullPath))
        {
            UE_LOG(LogTemp, Log, TEXT("LocalStudio: Deleted C++ file [%s]"), *FullPath);
            return true;
        }
        OutError = FString::Printf(TEXT("Failed to delete existing file: %s"), *FullPath);
        return false;
    }

    // File was already missing, consider deletion complete
    return true;
}

FString FLocalStudioCodeBuilder::ReadSourceFile(const FString& RelativeOrAbsolutePath)
{
    FString FullPath = FPaths::IsRelative(RelativeOrAbsolutePath)
        ? FPaths::Combine(FPaths::ProjectDir(), RelativeOrAbsolutePath)
        : RelativeOrAbsolutePath;

    FString Content;
    if (FFileHelper::LoadFileToString(Content, *FullPath))
    {
        return Content;
    }
    return FString();
}

FString FLocalStudioCodeBuilder::ScanProjectHeaders()
{
    FString SourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
    TArray<FString> FoundHeaders;

    IFileManager::Get().FindFilesRecursive(FoundHeaders, *SourceDir, TEXT("*.h"), true, false, false);

    FString ContextSummary = TEXT("=== EXISTING PROJECT HEADERS ===\n");
    for (const FString& HeaderPath : FoundHeaders)
    {
        FString RelativePath = HeaderPath;
        FPaths::MakePathRelativeTo(RelativePath, *FPaths::ProjectDir());

        FString FileContent = ReadSourceFile(HeaderPath);
        ContextSummary += FString::Printf(TEXT("\n--- FILE: %s ---\n%s\n"), *RelativePath, *FileContent);
    }

    return ContextSummary;
}

bool FLocalStudioCodeBuilder::TriggerLiveCompilation(FString& OutCompileLog)
{
    ILiveCodingModule* LiveCodingModule = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
    if (LiveCodingModule && LiveCodingModule->IsEnabledForSession())
    {
        LiveCodingModule->Compile();
        OutCompileLog = TEXT("Live Coding compilation initiated successfully.");
        return true;
    }

    OutCompileLog = TEXT("Live Coding is inactive or not enabled in this session.");
    return false;
}

FString FLocalStudioCodeBuilder::FindEngineHeader(const FString& ClassOrTypeName)
{
    const ULocalStudioSettings* Settings = GetDefault<ULocalStudioSettings>();
    if (!Settings || Settings->EnginePath.Path.IsEmpty())
    {
        return FString();
    }

    FString BasePath = Settings->EnginePath.Path;
    if (!BasePath.EndsWith(TEXT("Engine")))
    {
        BasePath = FPaths::Combine(BasePath, TEXT("Engine"));
    }

    FString EngineSourceDir = FPaths::Combine(BasePath, TEXT("Source/Runtime"));

    FString HeaderFileName = ClassOrTypeName;
    if (HeaderFileName.StartsWith(TEXT("U")) || HeaderFileName.StartsWith(TEXT("A")) || HeaderFileName.StartsWith(TEXT("F")))
    {
        HeaderFileName = HeaderFileName.RightChop(1);
    }
    HeaderFileName += TEXT(".h");

    TArray<FString> FoundFiles;
    IFileManager::Get().FindFilesRecursive(FoundFiles, *EngineSourceDir, *HeaderFileName, true, false);

    if (FoundFiles.Num() > 0)
    {
        FString FilePath = FoundFiles[0];
        FString IncludePath = FilePath;
        int32 PublicIdx = IncludePath.Find(TEXT("/Public/"));
        if (PublicIdx != INDEX_NONE)
        {
            IncludePath = IncludePath.Mid(PublicIdx + 8);
        }
        else
        {
            IncludePath = FPaths::GetCleanFilename(FilePath);
        }

        FString FileContent;
        if (FFileHelper::LoadFileToString(FileContent, *FilePath))
        {
            return FString::Printf(TEXT("// PROPER INCLUDE: #include \"%s\"\n\n%s"), *IncludePath, *FileContent.Left(2000));
        }
    }

    return FString();
}

void FLocalStudioCodeBuilder::EnsureBuildDependencies(const FString& Code)
{
    TArray<FString> RequiredModules;

    if (Code.Contains(TEXT("UEnhancedInputComponent")) || Code.Contains(TEXT("UInputAction")))
    {
        RequiredModules.AddUnique(TEXT("\"EnhancedInput\""));
    }
    if (Code.Contains(TEXT("UUserWidget")) || Code.Contains(TEXT("UWidget")))
    {
        RequiredModules.AddUnique(TEXT("\"UMG\""));
        RequiredModules.AddUnique(TEXT("\"Slate\""));
        RequiredModules.AddUnique(TEXT("\"SlateCore\""));
    }
    if (Code.Contains(TEXT("UAIBlueprintHelperLibrary")) || Code.Contains(TEXT("AAIController")))
    {
        RequiredModules.AddUnique(TEXT("\"AIModule\""));
    }

    if (RequiredModules.Num() == 0)
    {
        return;
    }

    // Locate the project's .Build.cs file
    TArray<FString> FoundBuildFiles;
    IFileManager::Get().FindFilesRecursive(FoundBuildFiles, *FPaths::GameSourceDir(), TEXT("*.Build.cs"), true, false);

    if (FoundBuildFiles.Num() == 0)
    {
        return;
    }

    FString BuildCsPath = FoundBuildFiles[0];
    FString BuildCsContent;
    if (!FFileHelper::LoadFileToString(BuildCsContent, *BuildCsPath))
    {
        return;
    }

    bool bModified = false;
    for (const FString& ModuleName : RequiredModules)
    {
        if (!BuildCsContent.Contains(ModuleName))
        {
            int32 TargetIdx = BuildCsContent.Find(TEXT("PublicDependencyModuleNames.AddRange("));
            if (TargetIdx != INDEX_NONE)
            {
                int32 ArrayEndIdx = BuildCsContent.Find(TEXT("});"), ESearchCase::IgnoreCase, ESearchDir::FromStart, TargetIdx);
                if (ArrayEndIdx != INDEX_NONE)
                {
                    FString Insertion = FString::Printf(TEXT(", %s"), *ModuleName);
                    BuildCsContent.InsertAt(ArrayEndIdx, Insertion);
                    bModified = true;
                }
            }
        }
    }

    if (bModified)
    {
        FFileHelper::SaveStringToFile(BuildCsContent, *BuildCsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Log, TEXT("LocalStudio: Updated Build.cs with missing modules: %s"), *BuildCsPath);
    }
}

FString FLocalStudioCodeBuilder::BuildParentClassReflectionContext(const FString& ParentClassName)
{
    FString ReflectionSummary;
    FParentClassReflectionInfo ReflectionInfo = GetParentReflectionInfo(ParentClassName);

    UClass* FoundClass = FindFirstObject<UClass>(*ParentClassName, EFindFirstObjectOptions::None);
    if (!FoundClass)
    {
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->GetName() == ParentClassName ||
                It->GetName() == (TEXT("A") + ParentClassName) ||
                It->GetName() == (TEXT("U") + ParentClassName))
            {
                FoundClass = *It;
                break;
            }
        }
    }

    ReflectionSummary += FString::Printf(TEXT("=== PARENT CLASS REFLECTION SUMMARY [%s] ===\n"), *ParentClassName);

    if (ReflectionInfo.PropertyNames.Num() > 0)
    {
        ReflectionSummary += TEXT("PARENTS ALREADY DECLARE THESE PROPERTIES (DO NOT RE-DECLARE THEM IN DERIVED CLASSES):\n");
        for (const FString& PropName : ReflectionInfo.PropertyNames)
        {
            ReflectionSummary += FString::Printf(TEXT(" - %s\n"), *PropName);
        }
    }

    if (ReflectionInfo.RPCNames.Num() > 0)
    {
        ReflectionSummary += TEXT("PARENTS ALREADY DECLARE THESE RPCS (DO NOT RE-DECLARE UFUNCTION OR HEADERS FOR THEM):\n");
        for (const FString& RPCName : ReflectionInfo.RPCNames)
        {
            ReflectionSummary += FString::Printf(TEXT(" - %s\n"), *RPCName);
        }
    }

    if (FoundClass)
    {
        ReflectionSummary += TEXT("AVAILABLE REFLECTED VIRTUAL FUNCTIONS & SIGNATURES:\n");
        for (TFieldIterator<UFunction> FuncIt(FoundClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
        {
            UFunction* Func = *FuncIt;
            if (Func)
            {
                FString ParamList;
                for (TFieldIterator<FProperty> PropIt(Func); PropIt; ++PropIt)
                {
                    FProperty* Prop = *PropIt;
                    if (Prop->HasAnyPropertyFlags(CPF_Parm))
                    {
                        ParamList += FString::Printf(TEXT("%s %s, "), *Prop->GetCPPType(), *Prop->GetName());
                    }
                }
                if (ParamList.EndsWith(TEXT(", ")))
                {
                    ParamList.LeftChopInline(2);
                }

                if (ReflectionSummary.Len() > 2500)
                {
                    ReflectionSummary += TEXT("\n...[Truncated Reflected Functions]");
                    break;
                }

                ReflectionSummary += FString::Printf(TEXT(" - %s(%s)\n"), *Func->GetName(), *ParamList);
            }
        }
    }

    return ReflectionSummary;
}

FLocalStudioCodeBuilder::FParentClassReflectionInfo FLocalStudioCodeBuilder::GetParentReflectionInfo(const FString& ParentClassName)
{
    FParentClassReflectionInfo Info;

    // 1. Check in-memory engine reflection first
    UClass* TargetClass = FindFirstObject<UClass>(*ParentClassName, EFindFirstObjectOptions::None);
    if (!TargetClass)
    {
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->GetName() == ParentClassName ||
                It->GetName() == (TEXT("A") + ParentClassName) ||
                It->GetName() == (TEXT("U") + ParentClassName))
            {
                TargetClass = *It;
                break;
            }
        }
    }

    if (TargetClass)
    {
        for (TFieldIterator<FProperty> PropIt(TargetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            if (Prop)
            {
                Info.PropertyNames.Add(Prop->GetName());
                if (Prop->IsA<FObjectProperty>())
                {
                    Info.ComponentNames.Add(Prop->GetName());
                }
            }
        }

        for (TFieldIterator<UFunction> FuncIt(TargetClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
        {
            UFunction* Func = *FuncIt;
            if (Func)
            {
                Info.FunctionNames.Add(Func->GetName());
                if (Func->HasAnyFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetClient | FUNC_NetMulticast))
                {
                    Info.RPCNames.Add(Func->GetName());
                }
            }
        }
    }

    // 2. Scan disk header files for custom project parent classes
    FString SourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
    TArray<FString> FoundHeaders;
    IFileManager::Get().FindFilesRecursive(FoundHeaders, *SourceDir, TEXT("*.h"), true, false, false);

    FString ParentHeaderName = ParentClassName;
    if (ParentHeaderName.StartsWith(TEXT("A")) || ParentHeaderName.StartsWith(TEXT("U")))
    {
        ParentHeaderName = ParentHeaderName.RightChop(1);
    }

    for (const FString& HeaderPath : FoundHeaders)
    {
        if (FPaths::GetBaseFilename(HeaderPath).Equals(ParentHeaderName, ESearchCase::IgnoreCase))
        {
            FString HeaderContent = ReadSourceFile(HeaderPath);
            GatherParentClassDataFromHeader(HeaderContent, ParentClassName, Info);
            break;
        }
    }

    return Info;
}

void FLocalStudioCodeBuilder::GatherParentClassDataFromHeader(const FString& HeaderContent, const FString& ParentClassName, FParentClassReflectionInfo& OutInfo)
{
    TArray<FString> Lines;
    HeaderContent.ParseIntoArrayLines(Lines, false);

    bool bLastLineWasUPROPERTY = false;
    bool bLastLineWasUFUNCTION = false;
    bool bIsRPC = false;

    // Clean prefix for class comparison (e.g. AMasterCharacter -> MasterCharacter)
    FString CleanParentName = ParentClassName;
    if (CleanParentName.StartsWith(TEXT("A")) || CleanParentName.StartsWith(TEXT("U")) || CleanParentName.StartsWith(TEXT("F")))
    {
        CleanParentName = CleanParentName.RightChop(1);
    }

    for (int32 i = 0; i < Lines.Num(); ++i)
    {
        FString Line = Lines[i].TrimStartAndEnd();
        if (Line.IsEmpty()) continue;

        // 1. Track Macros
        if (Line.StartsWith(TEXT("UPROPERTY")))
        {
            bLastLineWasUPROPERTY = true;
            continue;
        }

        if (Line.StartsWith(TEXT("UFUNCTION")))
        {
            bLastLineWasUFUNCTION = true;
            bIsRPC = Line.Contains(TEXT("Server")) || Line.Contains(TEXT("Client")) || Line.Contains(TEXT("NetMulticast"));
            continue;
        }

        // 2. Process Property Variables right after UPROPERTY
        if (bLastLineWasUPROPERTY)
        {
            bLastLineWasUPROPERTY = false;

            FString VarLine = Line;
            VarLine.ReplaceInline(TEXT(";"), TEXT(""));
            int32 SpaceIdx = VarLine.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
            if (SpaceIdx != INDEX_NONE)
            {
                FString VarName = VarLine.RightChop(SpaceIdx + 1).TrimStartAndEnd();
                VarName.RemoveFromStart(TEXT("*"));
                if (!VarName.IsEmpty())
                {
                    OutInfo.PropertyNames.Add(VarName);
                }
            }
            continue;
        }

        // 3. Process Functions (Collect both UFUNCTION and raw C++ virtual/member functions)
        if (Line.Contains(TEXT("(")) && Line.Contains(TEXT(")")) && !Line.StartsWith(TEXT("//")))
        {
            // Exclude UCLASS, GENERATED_BODY, macros, and constructors
            if (!Line.StartsWith(TEXT("UCLASS")) &&
                !Line.StartsWith(TEXT("GENERATED_BODY")) &&
                !Line.StartsWith(TEXT("UPROPERTY")) &&
                !Line.StartsWith(TEXT("UFUNCTION")))
            {
                int32 ParenIdx = Line.Find(TEXT("("));
                FString Sub = Line.Left(ParenIdx).TrimEnd();
                int32 SpaceIdx = Sub.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
                FString FuncName = (SpaceIdx != INDEX_NONE) ? Sub.RightChop(SpaceIdx + 1) : Sub;

                // Clean up references or pointers in return types attached to name
                FuncName.RemoveFromStart(TEXT("*"));
                FuncName.RemoveFromStart(TEXT("&"));

                if (!FuncName.IsEmpty() && FuncName != ParentClassName && FuncName != CleanParentName)
                {
                    // TSet uses .Add() to ensure uniqueness
                    OutInfo.FunctionNames.Add(FuncName);
                    if (bIsRPC)
                    {
                        OutInfo.RPCNames.Add(FuncName);
                    }
                }
            }
            bLastLineWasUFUNCTION = false;
            bIsRPC = false;
        }
    }
}

