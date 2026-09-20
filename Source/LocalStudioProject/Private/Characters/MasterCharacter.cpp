#include "Characters/MasterCharacter.h"
#include "Components/StatsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AMasterCharacter::AMasterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create and attach the stats component
	StatsComponent = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));

	// Set up the character's movement
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
}

// Called when the game starts or when spawned
void AMasterCharacter::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AMasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AMasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// Getters for stats
float AMasterCharacter::GetHealth() const
{
	return StatsComponent ? StatsComponent->Health : 0.0f;
}

float AMasterCharacter::GetMaxHealth() const
{
	return StatsComponent ? StatsComponent->MaxHealth : 0.0f;
}

float AMasterCharacter::GetStamina() const
{
	return StatsComponent ? StatsComponent->Stamina : 0.0f;
}

float AMasterCharacter::GetMaxStamina() const
{
	return StatsComponent ? StatsComponent->MaxStamina : 0.0f;
}

float AMasterCharacter::GetHunger() const
{
	return StatsComponent ? StatsComponent->Hunger : 0.0f;
}

float AMasterCharacter::GetMaxHunger() const
{
	return StatsComponent ? StatsComponent->MaxHunger : 0.0f;
}

float AMasterCharacter::GetThirst() const
{
	return StatsComponent ? StatsComponent->Thirst : 0.0f;
}

float AMasterCharacter::GetMaxThirst() const
{
	return StatsComponent ? StatsComponent->MaxThirst : 0.0f;
}

// Health modification functions
void AMasterCharacter::SetHealth(float NewHealth)
{
	if (StatsComponent)
	{
		StatsComponent->SetHealth(NewHealth);
	}
}

void AMasterCharacter::ModifyHealth(float Delta)
{
	if (StatsComponent)
	{
		StatsComponent->ModifyHealth(Delta);
	}
}

// Stamina modification functions
void AMasterCharacter::SetStamina(float NewStamina)
{
	if (StatsComponent)
	{
		StatsComponent->SetStamina(NewStamina);
	}
}

void AMasterCharacter::ModifyStamina(float Delta)
{
	if (StatsComponent)
	{
		StatsComponent->ModifyStamina(Delta);
	}
}

// Hunger modification functions
void AMasterCharacter::SetHunger(float NewHunger)
{
	if (StatsComponent)
	{
		StatsComponent->SetHunger(NewHunger);
	}
}

void AMasterCharacter::ModifyHunger(float Delta)
{
	if (StatsComponent)
	{
		StatsComponent->ModifyHunger(Delta);
	}
}

// Thirst modification functions
void AMasterCharacter::SetThirst(float NewThirst)
{
	if (StatsComponent)
	{
		StatsComponent->SetThirst(NewThirst);
	}
}

void AMasterCharacter::ModifyThirst(float Delta)
{
	if (StatsComponent)
	{
		StatsComponent->ModifyThirst(Delta);
	}
}