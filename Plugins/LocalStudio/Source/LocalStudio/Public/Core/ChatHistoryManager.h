#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Dom/JsonObject.h"
#include "ChatHistoryManager.generated.h"

USTRUCT(BlueprintType)
struct FChatMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Sender;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Message;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FDateTime Timestamp;

    // Serialization methods (implemented in ChatHistoryManager.cpp)
    TSharedPtr<FJsonObject> ToJsonObject() const;
    static FChatMessage FromJsonObject(const TSharedPtr<FJsonObject>& Obj);

};

UCLASS()
class LOCALSTUDIO_API UChatHistoryManager : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    void AddChatMessage(const FString& Sender, const FString& Message);
    const TArray<FChatMessage>& GetChatHistory() const { return ChatHistory; }
    void ClearHistory();

    bool SaveHistoryToFile();
    bool LoadHistoryFromFile();

private:
    FString GetSaveFilePath() const;

    TArray<FChatMessage> ChatHistory;
    int32 MaxHistoryCount = 50;
};