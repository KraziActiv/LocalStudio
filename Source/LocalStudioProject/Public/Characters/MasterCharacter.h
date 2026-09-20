#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MasterCharacter.generated.h"

class UStatsComponent;

UCLASS(Blueprintable, BlueprintType)
class LOCALSTUDIOPROJECT_API AMasterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AMasterCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Stats component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	UStatsComponent* StatsComponent;

	// Getters for stats
	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetStamina() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetMaxStamina() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetHunger() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetMaxHunger() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetThirst() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetMaxThirst() const;

	// Health modification functions
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetHealth(float NewHealth);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyHealth(float Delta);

	// Stamina modification functions
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetStamina(float NewStamina);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyStamina(float Delta);

	// Hunger modification functions
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetHunger(float NewHunger);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyHunger(float Delta);

	// Thirst modification functions
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetThirst(float NewThirst);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyThirst(float Delta);
};