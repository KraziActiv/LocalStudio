#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SurvivalGameState.generated.h"

class APlayerState;

UCLASS(Blueprintable, BlueprintType)
class LOCALSTUDIOPROJECT_API ASurvivalGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASurvivalGameState(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// Replicated game state properties
	UPROPERTY(Replicated)
	float GameTime;

	UPROPERTY(Replicated)
	float WeatherIntensity;

	UPROPERTY(Replicated)
	FString GameRules;

	// Server-only properties (not replicated)
	float TimeOfDay;

	// Getters for game state
	UFUNCTION(BlueprintCallable, Category = "Game State")
	float GetGameTime() const;

	UFUNCTION(BlueprintCallable, Category = "Game State")
	float GetWeatherIntensity() const;

	UFUNCTION(BlueprintCallable, Category = "Game State")
	FString GetGameRules() const;

	// Setters for game state
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void SetGameTime(float NewGameTime);

	UFUNCTION(BlueprintCallable, Category = "Game State")
	void SetWeatherIntensity(float NewWeatherIntensity);

	UFUNCTION(BlueprintCallable, Category = "Game State")
	void SetGameRules(const FString& NewGameRules);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};