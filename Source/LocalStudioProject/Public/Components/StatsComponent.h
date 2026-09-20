#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatsComponent.generated.h"

// Forward declarations
class AActor;

/**
 * Delegate for when any vital changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVitalChanged, float, NewValue);

/**
 * Delegate for when health changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChanged, float, NewHealth);

/**
 * Delegate for when stamina changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaChanged, float, NewStamina);

/**
 * Delegate for when hunger changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHungerChanged, float, NewHunger);

/**
 * Delegate for when thirst changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThirstChanged, float, NewThirst);

/**
 * Delegate for when oxygen changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOxygenChanged, float, NewOxygen);

/**
 * Delegate for when temperature changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTemperatureChanged, float, NewTemperature);

/**
 * Delegate for when pain changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPainChanged, float, NewPain);

/**
 * Delegate for when immunity changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnImmunityChanged, float, NewImmunity);

/**
 * Delegate for when energy changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnergyChanged, float, NewEnergy);

/**
 * Delegate for when nutrients changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNutrientsChanged, float, NewNutrients);

/**
 * Delegate for when vitamins changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVitaminsChanged, float, NewVitamins);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LOCALSTUDIOPROJECT_API UStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStatsComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Health properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Health;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth;

	// Stamina properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Stamina;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxStamina;

	// Hunger properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Hunger;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHunger;

	// Thirst properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Thirst;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxThirst;

	// Oxygen properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Oxygen;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxOxygen;

	// Temperature properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Temperature;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxTemperature;

	// Pain properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Pain;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxPain;

	// Immunity properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Immunity;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxImmunity;

	// Energy properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Energy;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxEnergy;

	// Nutrients properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Nutrients;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxNutrients;

	// Vitamins properties
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Vitamins;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxVitamins;

	// Delegates for vital changes
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnVitalChanged OnVitalChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnStaminaChanged OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnHungerChanged OnHungerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnThirstChanged OnThirstChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnOxygenChanged OnOxygenChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnTemperatureChanged OnTemperatureChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnPainChanged OnPainChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnImmunityChanged OnImmunityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnEnergyChanged OnEnergyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnNutrientsChanged OnNutrientsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnVitaminsChanged OnVitaminsChanged;

	// Functions to modify vitals
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetHealth(float NewHealth);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetStamina(float NewStamina);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetHunger(float NewHunger);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetThirst(float NewThirst);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetOxygen(float NewOxygen);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetTemperature(float NewTemperature);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetPain(float NewPain);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetImmunity(float NewImmunity);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetEnergy(float NewEnergy);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetNutrients(float NewNutrients);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetVitamins(float NewVitamins);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyHealth(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyStamina(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyHunger(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyThirst(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyOxygen(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyTemperature(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyPain(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyImmunity(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyEnergy(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyNutrients(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ModifyVitamins(float Delta);

protected:
	// Replication functions
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_Stamina();

	UFUNCTION()
	void OnRep_Hunger();

	UFUNCTION()
	void OnRep_Thirst();

	UFUNCTION()
	void OnRep_Oxygen();

	UFUNCTION()
	void OnRep_Temperature();

	UFUNCTION()
	void OnRep_Pain();

	UFUNCTION()
	void OnRep_Immunity();

	UFUNCTION()
	void OnRep_Energy();

	UFUNCTION()
	void OnRep_Nutrients();

	UFUNCTION()
	void OnRep_Vitamins();

	// Server RPC functions
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyHealth(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyStamina(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyHunger(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyThirst(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyOxygen(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyTemperature(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyPain(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyImmunity(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyEnergy(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyNutrients(float Delta);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerModifyVitamins(float Delta);
};