#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SurvivalGameMode.generated.h"

UCLASS(Blueprintable, BlueprintType)
class LOCALSTUDIOPROJECT_API ASurvivalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASurvivalGameMode(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
};