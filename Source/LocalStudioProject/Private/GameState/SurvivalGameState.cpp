#include "GameState/SurvivalGameState.h"
#include "Net/UnrealNetwork.h"

ASurvivalGameState::ASurvivalGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set default values
	GameTime = 0.0f;
	WeatherIntensity = 0.0f;
	TimeOfDay = 0.0f;
	GameRules = TEXT("Default Survival Rules");

	// Set this game state to replicate
	bReplicates = true;
}

void ASurvivalGameState::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

void ASurvivalGameState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update game time
	if (HasAuthority())
	{
		GameTime += DeltaTime;
	}
}

float ASurvivalGameState::GetGameTime() const
{
	return GameTime;
}

float ASurvivalGameState::GetWeatherIntensity() const
{
	return WeatherIntensity;
}

FString ASurvivalGameState::GetGameRules() const
{
	return GameRules;
}

void ASurvivalGameState::SetGameTime(float NewGameTime)
{
	if (HasAuthority())
	{
		GameTime = NewGameTime;
	}
}

void ASurvivalGameState::SetWeatherIntensity(float NewWeatherIntensity)
{
	if (HasAuthority())
	{
		WeatherIntensity = NewWeatherIntensity;
	}
}

void ASurvivalGameState::SetGameRules(const FString& NewGameRules)
{
	if (HasAuthority())
	{
		GameRules = NewGameRules;
	}
}

void ASurvivalGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASurvivalGameState, GameTime);
	DOREPLIFETIME(ASurvivalGameState, WeatherIntensity);
	DOREPLIFETIME(ASurvivalGameState, GameRules);
}