#pragma once

#include "CoreMinimal.h"
#include "ILocalStudioBuilder.h"

class LOCALSTUDIO_API FLocalStudioBuilderRegistry
{
public:
    static FLocalStudioBuilderRegistry& Get()
    {
        static FLocalStudioBuilderRegistry Instance;
        return Instance;
    }

    void RegisterBuilder(TSharedRef<ILocalStudioBuilder> Builder)
    {
        FName ID = Builder->GetBuilderID();
        if (!RegisteredBuilders.Contains(ID))
        {
            RegisteredBuilders.Add(ID, Builder);
            BuilderIDs.Add(ID);
            UE_LOG(LogTemp, Log, TEXT("[LocalStudio] Registered Modular Builder: %s"), *ID.ToString());
        }
    }

    TSharedPtr<ILocalStudioBuilder> GetBuilder(FName BuilderID) const
    {
        const TSharedRef<ILocalStudioBuilder>* Found = RegisteredBuilders.Find(BuilderID);
        return Found ? Found->ToSharedPtr() : nullptr;
    }

    const TArray<FName>& GetRegisteredBuilderIDs() const
    {
        return BuilderIDs;
    }

    const TMap<FName, TSharedRef<ILocalStudioBuilder>>& GetAllBuilders() const
    {
        return RegisteredBuilders;
    }

    /** Clears registered builders on plugin shutdown or module reload */
    void UnregisterAll()
    {
        RegisteredBuilders.Empty();
        BuilderIDs.Empty();
    }

private:
    TMap<FName, TSharedRef<ILocalStudioBuilder>> RegisteredBuilders;
    TArray<FName> BuilderIDs;
};