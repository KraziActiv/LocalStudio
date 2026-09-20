#include "GameModes/SurvivalGameMode.h"
#include "Characters/SurvivalCharacter.h"

ASurvivalGameMode::ASurvivalGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set the default pawn class to our survival character
	DefaultPawnClass = ASurvivalCharacter::StaticClass();
}

void ASurvivalGameMode::BeginPlay()
{
	Super::BeginPlay();
}