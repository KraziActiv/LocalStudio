#pragma once

#include "CoreMinimal.h"
#include "Characters/MasterCharacter.h"
#include "SurvivalCharacter.generated.h"

class ACharacterHUD;

class UStatsComponent;

class UInputComponent;

class UUserWidget;

class UCanvasPanel;

UCLASS(Blueprintable, BlueprintType)
class LOCALSTUDIOPROJECT_API ASurvivalCharacter : public AMasterCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASurvivalCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// Experience Points
	UPROPERTY(Replicated)
	int32 XP;

	// HUD Reference
	UPROPERTY()
	ACharacterHUD* CharacterHUD;

	// Widget for HUD
	UPROPERTY()
	UUserWidget* CharacterHUDWidget;

	// Canvas panel for HUD
	UPROPERTY()
	UCanvasPanel* HUDPanel;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};