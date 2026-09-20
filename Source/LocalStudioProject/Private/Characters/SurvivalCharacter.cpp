#include "Characters/SurvivalCharacter.h"
#include "Net/UnrealNetwork.h"
#include "HUD/CharacterHUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"

// Sets default values
ASurvivalCharacter::ASurvivalCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Initialize XP
	XP = 0;

	// Initialize HUD reference
	CharacterHUD = nullptr;

	// Initialize widget references
	CharacterHUDWidget = nullptr;
	HUDPanel = nullptr;
}

// Called when the game starts or when spawned
void ASurvivalCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Create and attach HUD
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// Create HUD if not already created
		if (!CharacterHUD)
		{
			CharacterHUD = NewObject<ACharacterHUD>(PC);
			if (CharacterHUD)
			{
				CharacterHUD->SetMasterCharacter(this);
				// Use proper method to add HUD
				if (UUserWidget* HUDWidget = CreateWidget<UUserWidget>(PC, CharacterHUD->GetClass()))
				{
					CharacterHUDWidget = HUDWidget;
					// Add to viewport
					HUDWidget->AddToViewport();
				}
			}
		}
	}
}

// Called every frame
void ASurvivalCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ASurvivalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ASurvivalCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASurvivalCharacter, XP);
}