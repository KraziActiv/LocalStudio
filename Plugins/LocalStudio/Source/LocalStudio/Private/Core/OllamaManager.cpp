#include "Core/OllamaManager.h"
#include "Core/LocalStudioSettings.h"
#include "Core/SLocalStudioChatPanel.h"
#include "Core/ILocalStudioBuilder.h"
#include "Core/BuilderRegistry.h"
#include "Core/LocalStudioJsonUtils.h"

#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UOllamaManager* UOllamaManager::Instance = nullptr;

UOllamaManager::UOllamaManager()
{
    bIsInitialized = false;
    Instance = this;
}

UOllamaManager* UOllamaManager::Get()
{
    return Instance;
}

void UOllamaManager::ClearActiveContext()
{
}

bool UOllamaManager::InitializeOllama()
{
    const ULocalStudioSettings* StudioSettings = GetDefault<ULocalStudioSettings>();
    if (!StudioSettings) return false;

    OllamaHost = StudioSettings->OllamaHost;
    OllamaPort = StudioSettings->OllamaPort;

    UE_LOG(LogTemp, Log, TEXT("Local Studio: Connecting directly to server %s:%d..."), *OllamaHost, OllamaPort);
    FetchAvailableModels();

    return true;
}

void UOllamaManager::OnPingCheckCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        UE_LOG(LogTemp, Log, TEXT("Local Studio: Server instance running."));

        const ULocalStudioSettings* StudioSettings = GetDefault<ULocalStudioSettings>();
        if (StudioSettings)
        {
            OllamaHost = StudioSettings->OllamaHost;
            OllamaPort = StudioSettings->OllamaPort;
        }

        bIsInitialized = true;
        FetchAvailableModels();
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Local Studio: Server not responding. Attempting launch..."));
    LaunchServerProcess();
}

void UOllamaManager::LaunchServerProcess()
{
    const ULocalStudioSettings* StudioSettings = GetDefault<ULocalStudioSettings>();
    if (!StudioSettings) return;

    FString PortString = FString::FromInt(StudioSettings->OllamaPort);
    ServerUrl = FString::Printf(TEXT("http://%s:%s/"), *StudioSettings->OllamaHost, *PortString);

    bool bIsLocalhost = ServerUrl.Contains(TEXT("127.0.0.1")) ||
        ServerUrl.Contains(TEXT("localhost")) ||
        ServerUrl.Contains(TEXT("0.0.0.0")) ||
        StudioSettings->OllamaHost.IsEmpty();

    if (!bIsLocalhost)
    {
        UE_LOG(LogTemp, Warning, TEXT("Local Studio: Remote host offline. Falling back to LOCALHOST."));
        OllamaHost = TEXT("127.0.0.1");
        OllamaPort = StudioSettings->OllamaPort;
    }
    else
    {
        OllamaHost = StudioSettings->OllamaHost.IsEmpty() ? TEXT("127.0.0.1") : StudioSettings->OllamaHost;
        OllamaPort = StudioSettings->OllamaPort;
    }

    FString TargetExecutable = TEXT("cmd.exe");
    FString LaunchArguments;

    if (StudioSettings->ComputeDevice == EOllamaComputeDevice::CPU)
    {
        LaunchArguments = TEXT("/c \"set CUDA_VISIBLE_DEVICES=-1&& set ROCM_VISIBLE_DEVICES=-1&& set OLLAMA_VULKAN=0&& set OLLAMA_NOPRUNE=1&& set OLLAMA_NUM_GPU=0&& set OLLAMA_NUM_PARALLEL=1&& ollama serve\"");
    }
    else
    {
        LaunchArguments = TEXT("/c \"set OLLAMA_NUM_PARALLEL=1&& ollama serve\"");
    }

    uint32 NativeProcessID = 0;
    FProcHandle ServerHandle = FPlatformProcess::CreateProc(
        *TargetExecutable,
        *LaunchArguments,
        true, true, true,
        &NativeProcessID, 0, nullptr, nullptr
    );

    if (ServerHandle.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("Local Studio: Server spawned PID: %d"), NativeProcessID);
        bIsInitialized = true;
    }
}

FString UOllamaManager::LoadEngineHeaderSnippet(const FString& RelativeEnginePath) const
{
    FString BaseEngineDir;

    const ULocalStudioSettings* StudioSettings = GetDefault<ULocalStudioSettings>();
    if (StudioSettings && !StudioSettings->EnginePathOverride.IsEmpty())
    {
        BaseEngineDir = StudioSettings->EnginePathOverride;
    }
    else
    {
        // Automatic engine directory detection fallback
        BaseEngineDir = FPaths::EngineDir();
    }

    FString FullFilePath = FPaths::Combine(BaseEngineDir, RelativeEnginePath);

    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullFilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[OllamaManager] Specified Engine file not found: %s"), *FullFilePath);
        return TEXT("");
    }

    FString FileContent;
    if (FFileHelper::LoadFileToString(FileContent, *FullFilePath))
    {
        if (FileContent.Len() > 4000)
        {
            FileContent = FileContent.Left(4000) + TEXT("\n... [Engine Header Content Truncated for Context Window] ...");
        }
        return FileContent;
    }

    return TEXT("");
}

FString UOllamaManager::GetEngineContextContextForPrompt(const FString& TargetTopic) const
{
    FString EngineContext = TEXT("");

    // Target relative path within Engine directory
    FString TargetHeader = TEXT("Source/Runtime/Engine/Classes/Engine/Engine.h");

    FString HeaderContent = LoadEngineHeaderSnippet(TargetHeader);
    if (!HeaderContent.IsEmpty())
    {
        EngineContext += FString::Printf(TEXT("\n\n--- UNREAL ENGINE REFERENCE DEFINITIONS (%s) ---\n%s\n--- END ENGINE REFERENCE ---"),
            *TargetHeader, *HeaderContent);
    }

    return EngineContext;
}

void UOllamaManager::OnConnectionTestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        bIsInitialized = true;
        OnOllamaConnectionChanged.ExecuteIfBound(true);
        FetchAvailableModels();
    }
    else
    {
        OnOllamaConnectionChanged.ExecuteIfBound(false);
    }
}

void UOllamaManager::FetchAvailableModels()
{
    FString Url = FString::Printf(TEXT("http://%s:%d/api/tags"), *OllamaHost, OllamaPort);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("GET"));
    Request->OnProcessRequestComplete().BindUObject(this, &UOllamaManager::OnModelsReceived);

    Request->ProcessRequest();
}

void UOllamaManager::OnModelsReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    TArray<TSharedPtr<FString>> ParsedModels;

    if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        TSharedPtr<FJsonObject> JsonObject;
        if (FLocalStudioJsonUtils::DeserializeJsonObject(Response->GetContentAsString(), JsonObject))
        {
            const TArray<TSharedPtr<FJsonValue>>* ModelsArray;
            if (JsonObject->TryGetArrayField(TEXT("models"), ModelsArray))
            {
                for (const auto& Value : *ModelsArray)
                {
                    TSharedPtr<FJsonObject> ModelObj = Value->AsObject();
                    FString ModelName;
                    if (ModelObj.IsValid() && ModelObj->TryGetStringField(TEXT("name"), ModelName))
                    {
                        ParsedModels.Add(MakeShared<FString>(ModelName));
                    }
                }
            }
        }
    }

    if (ParsedModels.Num() == 0)
    {
        bool bIsAlreadyLocal = OllamaHost == TEXT("127.0.0.1") || OllamaHost == TEXT("localhost") || OllamaHost.IsEmpty();
        if (!bIsAlreadyLocal || OllamaHost.IsEmpty())
        {
            OllamaHost = TEXT("127.0.0.1");
        }

        LaunchServerProcess();

        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
            {
                FetchAvailableModels();
                return false;
            }), 2.5f);

        return;
    }

    CachedModelDropdownOptions = ParsedModels;
    OnAvailableModelsRefreshed.Broadcast(CachedModelDropdownOptions);
}

void UOllamaManager::RequestReasoning(const FString& Prompt, const FString& ProfileName, TFunction<void(const FString& Response, bool bSuccess)> Callback)
{
    FName BuilderID(*ProfileName);
    TSharedPtr<ILocalStudioBuilder> Builder = FLocalStudioBuilderRegistry::Get().GetBuilder(BuilderID);

    FString ModelName;
    const ULocalStudioSettings* StudioSettings = GetDefault<ULocalStudioSettings>();
    if (StudioSettings)
    {
        if (const FOllamaTaskProfile* Profile = StudioSettings->TaskProfiles.Find(ProfileName))
        {
            ModelName = Profile->ModelName;
        }
    }

    if (ModelName.IsEmpty() && Builder.IsValid())
    {
        ModelName = Builder->GetDefaultModel();
    }

    FString SystemSteering = TEXT("You are a helpful AI assistant.");
    if (StudioSettings)
    {
        SystemSteering = StudioSettings->GetAssembledSystemPrompt(ProfileName);
    }

    if (Builder.IsValid())
    {
        FString Guardrails = Builder->GetTechnicalGuardrails();
        if (!Guardrails.IsEmpty())
        {
            SystemSteering += TEXT("\n\n") + Guardrails;
        }
    }

    // --- INJECT ENGINE CONTEXT HERE ---
    FString EngineContext = GetEngineContextContextForPrompt(ProfileName);
    if (!EngineContext.IsEmpty())
    {
        SystemSteering += EngineContext;
    }

    TSharedRef<FJsonObject> RootJsonObject = MakeShared<FJsonObject>();
    RootJsonObject->SetStringField(TEXT("model"), ModelName);
    RootJsonObject->SetBoolField(TEXT("stream"), false);

    TArray<TSharedPtr<FJsonValue>> MessagesArray;

    TSharedPtr<FJsonObject> SystemMessage = MakeShared<FJsonObject>();
    SystemMessage->SetStringField(TEXT("role"), TEXT("system"));
    SystemMessage->SetStringField(TEXT("content"), SystemSteering);
    MessagesArray.Add(MakeShared<FJsonValueObject>(SystemMessage));

    TSharedPtr<FJsonObject> UserMessage = MakeShared<FJsonObject>();
    UserMessage->SetStringField(TEXT("role"), TEXT("user"));
    UserMessage->SetStringField(TEXT("content"), Prompt);
    MessagesArray.Add(MakeShared<FJsonValueObject>(UserMessage));

    RootJsonObject->SetArrayField(TEXT("messages"), MessagesArray);

    TSharedPtr<FJsonObject> OptionsObject = MakeShared<FJsonObject>();
    bool bIsConversational = ProfileName.Equals(TEXT("GeneralChat"), ESearchCase::IgnoreCase);

    float TempToUse = bIsConversational ? (StudioSettings ? StudioSettings->Temperature : 0.7f) : 0.1f;
    int32 MaxTokensToUse = StudioSettings ? StudioSettings->MaxTokens : 2500;
    int32 ContextWindowToUse = StudioSettings ? StudioSettings->ContextWindow : 8192;

    OptionsObject->SetNumberField(TEXT("temperature"), TempToUse);
    OptionsObject->SetNumberField(TEXT("num_predict"), MaxTokensToUse);
    OptionsObject->SetNumberField(TEXT("num_ctx"), ContextWindowToUse);
    OptionsObject->SetNumberField(TEXT("repeat_penalty"), 1.0f);
    OptionsObject->SetNumberField(TEXT("top_p"), 0.95f);

    RootJsonObject->SetObjectField(TEXT("options"), OptionsObject);

    FString RequestBody;
    FLocalStudioJsonUtils::SerializeJsonObjectToString(RootJsonObject, RequestBody);

    SendOllamaRequest(TEXT("/api/chat"), RequestBody, [Callback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            FString ResponseContent = TEXT("Error generating code: HTTP request failed or timed out.");
            bool bSuccess = false;

            if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
            {
                TSharedPtr<FJsonObject> OuterJson;
                if (FLocalStudioJsonUtils::DeserializeJsonObject(Response->GetContentAsString(), OuterJson))
                {
                    FString RawLlmContent;
                    const TSharedPtr<FJsonObject>* MessageObj = nullptr;

                    if (OuterJson->TryGetObjectField(TEXT("message"), MessageObj) && (*MessageObj)->TryGetStringField(TEXT("content"), RawLlmContent))
                    {
                        ResponseContent = RawLlmContent;
                        bSuccess = true;
                    }
                    else if (OuterJson->TryGetStringField(TEXT("response"), RawLlmContent))
                    {
                        ResponseContent = RawLlmContent;
                        bSuccess = true;
                    }
                }
            }

            if (Callback)
            {
                Callback(ResponseContent, bSuccess);
            }
        });
}

void UOllamaManager::RequestCode(const FString& Prompt, const FString& ModelType, TFunction<void(const FString& Response, bool bSuccess)> Callback)
{
    FString ModelName;
    const ULocalStudioSettings* StudioSettings = GetDefault<ULocalStudioSettings>();

    if (StudioSettings)
    {
        if (const FOllamaTaskProfile* Profile = StudioSettings->TaskProfiles.Find(TEXT("Code")))
        {
            ModelName = Profile->ModelName;
        }
    }

    if (ModelName.IsEmpty())
    {
        ModelName = TEXT("qwen3-coder:30b");
    }

    TSharedRef<FJsonObject> RootJsonObject = MakeShared<FJsonObject>();
    RootJsonObject->SetStringField(TEXT("model"), ModelName);
    RootJsonObject->SetStringField(TEXT("prompt"), Prompt);
    RootJsonObject->SetBoolField(TEXT("stream"), false);

    TSharedPtr<FJsonObject> OptionsObject = MakeShared<FJsonObject>();
    int32 MaxTokensToUse = StudioSettings ? StudioSettings->MaxTokens : 4096;
    int32 ContextWindowToUse = StudioSettings ? StudioSettings->ContextWindow : 8192;
    OptionsObject->SetNumberField(TEXT("num_predict"), MaxTokensToUse);
    OptionsObject->SetNumberField(TEXT("num_ctx"), ContextWindowToUse);
    RootJsonObject->SetObjectField(TEXT("options"), OptionsObject);

    RootJsonObject->SetStringField(TEXT("format"), TEXT("json"));

    FString RequestBody;
    FLocalStudioJsonUtils::SerializeJsonObjectToString(RootJsonObject, RequestBody);

    SendOllamaRequest(TEXT("/api/generate"), RequestBody, [Callback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            FString ResponseContent = TEXT("Error generating code.");
            bool bSuccess = false;

            if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
            {
                TSharedPtr<FJsonObject> OuterJson;
                if (FLocalStudioJsonUtils::DeserializeJsonObject(Response->GetContentAsString(), OuterJson))
                {
                    FString RawLlmContent;
                    if (OuterJson->TryGetStringField(TEXT("response"), RawLlmContent))
                    {
                        TSharedPtr<FJsonObject> InnerJson;
                        FString ErrorMsg;
                        if (FLocalStudioJsonUtils::ExtractAndParseJsonObject(RawLlmContent, InnerJson, ErrorMsg))
                        {
                            ResponseContent = RawLlmContent;
                            bSuccess = true;
                        }
                    }
                }
            }
            Callback(ResponseContent, bSuccess);
        });
}

TArray<TSharedPtr<FString>> UOllamaManager::GetCachedModelDropdownOptions() const
{
    return CachedModelDropdownOptions;
}

void UOllamaManager::SendOllamaRequest(
    const FString& Endpoint,
    const FString& JsonPayload,
    TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> OnComplete)
{
    FString Url = FString::Printf(TEXT("http://%s:%d%s"), *OllamaHost, OllamaPort, *Endpoint);

    int32 PayloadLength = JsonPayload.Len();
    int32 EstimatedInputTokens = PayloadLength / 4;

    UE_LOG(LogTemp, Warning, TEXT("[OllamaManager] Outgoing Payload: %d chars (~%d estimated input tokens)"),
        PayloadLength, EstimatedInputTokens);
    UE_LOG(LogTemp, Warning, TEXT("[OllamaManager] Payload Contents:\n%s"), *JsonPayload);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonPayload);

    if (const ULocalStudioSettings* StudioSettings = GetDefault<ULocalStudioSettings>())
    {
        Request->SetTimeout(StudioSettings->HttpTimeoutDuration);
        Request->SetActivityTimeout(StudioSettings->HttpActivityTimeout);
    }

    Request->OnProcessRequestComplete().BindLambda(
        [this, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                TSharedPtr<FJsonObject> JsonObj;
                FString RawResponse = Response->GetContentAsString();

                UE_LOG(LogTemp, Warning, TEXT("[OllamaManager] Raw Response Body: %s"), *RawResponse);

                if (FLocalStudioJsonUtils::DeserializeJsonObject(RawResponse, JsonObj))
                {
                    int32 PromptTokens = 0;
                    int32 GeneratedTokens = 0;
                    bool bHasPromptTokens = JsonObj->TryGetNumberField(TEXT("prompt_eval_count"), PromptTokens);
                    bool bHasEvalTokens = JsonObj->TryGetNumberField(TEXT("eval_count"), GeneratedTokens);

                    if (bHasPromptTokens || bHasEvalTokens)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[OllamaManager] Response Metrics -> Input Tokens: %d | Output Tokens Generated: %d"),
                            PromptTokens, GeneratedTokens);
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[OllamaManager] CRITICAL: JSON Deserialization failed! Raw Response length: %d"), RawResponse.Len());
                    UE_LOG(LogTemp, Error, TEXT("[OllamaManager] Raw Response (Failed Parsing): %s"), *RawResponse);
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[OllamaManager] HTTP Request Failed completely or timed out."));
            }

            OnComplete(Request, Response, bWasSuccessful);
        });

    Request->ProcessRequest();
}