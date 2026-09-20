#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"
#include "UObject/NoExportTypes.h"
#include "Core/StructEnums.h"
#include "OllamaManager.generated.h"

// Delegate signature for UI dropdown updates
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAvailableModelsRefreshed, const TArray<TSharedPtr<FString>>&);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnOllamaConnectionChanged, bool, bConnected);

UCLASS()
class LOCALSTUDIO_API UOllamaManager : public UObject
{
    GENERATED_BODY()

public:
    UOllamaManager();

    UFUNCTION(BlueprintCallable, Category = "Ollama")
    bool InitializeOllama();

    UFUNCTION(BlueprintCallable, Category = "Ollama")
    void FetchAvailableModels();

    UFUNCTION(BlueprintCallable, Category = "Ollama")
    static UOllamaManager* Get();

    void ClearActiveContext();

    /** Initiates reasoning processing chains from local model nodes */
    void RequestReasoning(const FString& Prompt, const FString& ProfileName, TFunction<void(const FString& Response, bool bSuccess)> Callback);
    void RequestCode(const FString& Prompt, const FString& ModelType, TFunction<void(const FString& Response, bool bSuccess)> Callback);

    /** The network address of the Ollama server (e.g., http://127.0.0.1:11434) */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Server Settings")
    FString ServerUrl = TEXT("http://127.0.0.1:11434");

    UPROPERTY()
    FOnOllamaConnectionChanged OnOllamaConnectionChanged;

    // Slate UI components bind to this to receive updated dropdown listings
    FOnAvailableModelsRefreshed OnAvailableModelsRefreshed;

    TArray<TSharedPtr<FString>> GetCachedModelDropdownOptions() const;

    // Cached shared pointers needed for SComboBox compatibility
    TArray<TSharedPtr<FString>> CachedModelDropdownOptions;

    FString GetEngineContextContextForPrompt(const FString& TargetTopic = TEXT("")) const;

private:

    FString OllamaHost = TEXT("127.0.0.1");
    int32 OllamaPort = 11434;
    bool bIsInitialized;

    void SendOllamaRequest(
        const FString& Endpoint,
        const FString& JsonPayload,
        TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> OnComplete);

    void OnConnectionTestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnModelsReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnPingCheckCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void LaunchServerProcess();

    static UOllamaManager* Instance;

    FString LoadEngineHeaderSnippet(const FString& RelativeEnginePath) const;
};