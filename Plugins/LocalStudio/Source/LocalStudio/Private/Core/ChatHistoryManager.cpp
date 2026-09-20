#include "Core/ChatHistoryManager.h"
#include "Core/LocalStudioJsonUtils.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"

// ==========================================
// FChatMessage Serialization Implementation
// ==========================================

TSharedPtr<FJsonObject> FChatMessage::ToJsonObject() const
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("Sender"), Sender);
    Obj->SetStringField(TEXT("Message"), Message);
    Obj->SetStringField(TEXT("Timestamp"), Timestamp.ToIso8601());
    return Obj;
}

FChatMessage FChatMessage::FromJsonObject(const TSharedPtr<FJsonObject>& Obj)
{
    FChatMessage Msg;
    if (Obj.IsValid())
    {
        if (!Obj->TryGetStringField(TEXT("Sender"), Msg.Sender))
        {
            Obj->TryGetStringField(TEXT("sender"), Msg.Sender);
        }

        if (!Obj->TryGetStringField(TEXT("Message"), Msg.Message))
        {
            Obj->TryGetStringField(TEXT("message"), Msg.Message);
        }

        FString TimeStr;
        if (Obj->TryGetStringField(TEXT("Timestamp"), TimeStr) || Obj->TryGetStringField(TEXT("timestamp"), TimeStr))
        {
            FDateTime::ParseIso8601(*TimeStr, Msg.Timestamp);
        }
    }
    return Msg;
}

// ==========================================
// UChatHistoryManager Implementation
// ==========================================

void UChatHistoryManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadHistoryFromFile();
}

FString UChatHistoryManager::GetSaveFilePath() const
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"), TEXT("ChatHistory.json"));
}

void UChatHistoryManager::AddChatMessage(const FString& Sender, const FString& Message)
{
    
    
    FChatMessage NewMsg;
    NewMsg.Sender = Sender;
    NewMsg.Message = Message;
    NewMsg.Timestamp = FDateTime::Now();

    ChatHistory.Add(NewMsg);

    if (ChatHistory.Num() > MaxHistoryCount)
    {
        int32 ItemsToRemove = ChatHistory.Num() - MaxHistoryCount;
        ChatHistory.RemoveAt(0, ItemsToRemove);
    }

    SaveHistoryToFile();
    
}

void UChatHistoryManager::ClearHistory()
{
    ChatHistory.Empty();
    SaveHistoryToFile();
}

bool UChatHistoryManager::SaveHistoryToFile()
{
    
    return true;

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    for (const FChatMessage& Msg : ChatHistory)
    {
        JsonArray.Add(MakeShared<FJsonValueObject>(Msg.ToJsonObject()));
    }

    FString OutputString;
    if (FLocalStudioJsonUtils::SerializeJsonArrayToString(JsonArray, OutputString))
    {
        FString FilePath = GetSaveFilePath();
        return FFileHelper::SaveStringToFile(OutputString, *FilePath);
    }

    return false;
    
}

bool UChatHistoryManager::LoadHistoryFromFile()
{
    FString FilePath = GetSaveFilePath();
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        return false;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        return false;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    if (FLocalStudioJsonUtils::DeserializeJsonArray(JsonString, JsonArray))
    {
        ChatHistory.Empty();
        for (const auto& Value : JsonArray)
        {
            if (TSharedPtr<FJsonObject> JsonObj = Value->AsObject())
            {
                ChatHistory.Add(FChatMessage::FromJsonObject(JsonObj));
            }
        }

        if (ChatHistory.Num() > MaxHistoryCount)
        {
            ChatHistory.RemoveAt(0, ChatHistory.Num() - MaxHistoryCount);
        }
        return true;
    }

    return false;
}