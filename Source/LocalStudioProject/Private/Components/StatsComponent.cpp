#include "Components/StatsComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

// Sets default values for this component's properties
UStatsComponent::UStatsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// These values are set to default human-like stats
	Health = 100.0f;
	MaxHealth = 100.0f;
	Stamina = 100.0f;
	MaxStamina = 100.0f;
	Hunger = 100.0f;
	MaxHunger = 100.0f;
	Thirst = 100.0f;
	MaxThirst = 100.0f;
	Oxygen = 100.0f;
	MaxOxygen = 100.0f;
	Temperature = 37.0f;
	MaxTemperature = 42.0f;
	Pain = 0.0f;
	MaxPain = 100.0f;
	Immunity = 100.0f;
	MaxImmunity = 100.0f;
	Energy = 100.0f;
	MaxEnergy = 100.0f;
	Nutrients = 100.0f;
	MaxNutrients = 100.0f;
	Vitamins = 100.0f;
	MaxVitamins = 100.0f;

	// Only replicate if we're on the server
	SetIsReplicatedByDefault(true);
}

// Called when the game starts
void UStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

// Called every frame
void UStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UStatsComponent::SetHealth(float NewHealth)
{
	if (GetOwner()->HasAuthority())
	{
		Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
		OnHealthChanged.Broadcast(Health);
		OnVitalChanged.Broadcast(Health);
	}
	else
	{
		// Server RPC call
		ServerModifyHealth(NewHealth - Health);
	}
}

void UStatsComponent::SetStamina(float NewStamina)
{
	if (GetOwner()->HasAuthority())
	{
		Stamina = FMath::Clamp(NewStamina, 0.0f, MaxStamina);
		OnStaminaChanged.Broadcast(Stamina);
		OnVitalChanged.Broadcast(Stamina);
	}
	else
	{
		// Server RPC call
		ServerModifyStamina(NewStamina - Stamina);
	}
}

void UStatsComponent::SetHunger(float NewHunger)
{
	if (GetOwner()->HasAuthority())
	{
		Hunger = FMath::Clamp(NewHunger, 0.0f, MaxHunger);
		OnHungerChanged.Broadcast(Hunger);
		OnVitalChanged.Broadcast(Hunger);
	}
	else
	{
		// Server RPC call
		ServerModifyHunger(NewHunger - Hunger);
	}
}

void UStatsComponent::SetThirst(float NewThirst)
{
	if (GetOwner()->HasAuthority())
	{
		Thirst = FMath::Clamp(NewThirst, 0.0f, MaxThirst);
		OnThirstChanged.Broadcast(Thirst);
		OnVitalChanged.Broadcast(Thirst);
	}
	else
	{
		// Server RPC call
		ServerModifyThirst(NewThirst - Thirst);
	}
}

void UStatsComponent::SetOxygen(float NewOxygen)
{
	if (GetOwner()->HasAuthority())
	{
		Oxygen = FMath::Clamp(NewOxygen, 0.0f, MaxOxygen);
		OnOxygenChanged.Broadcast(Oxygen);
		OnVitalChanged.Broadcast(Oxygen);
	}
	else
	{
		// Server RPC call
		ServerModifyOxygen(NewOxygen - Oxygen);
	}
}

void UStatsComponent::SetTemperature(float NewTemperature)
{
	if (GetOwner()->HasAuthority())
	{
		Temperature = FMath::Clamp(NewTemperature, 0.0f, MaxTemperature);
		OnTemperatureChanged.Broadcast(Temperature);
		OnVitalChanged.Broadcast(Temperature);
	}
	else
	{
		// Server RPC call
		ServerModifyTemperature(NewTemperature - Temperature);
	}
}

void UStatsComponent::SetPain(float NewPain)
{
	if (GetOwner()->HasAuthority())
	{
		Pain = FMath::Clamp(NewPain, 0.0f, MaxPain);
		OnPainChanged.Broadcast(Pain);
		OnVitalChanged.Broadcast(Pain);
	}
	else
	{
		// Server RPC call
		ServerModifyPain(NewPain - Pain);
	}
}

void UStatsComponent::SetImmunity(float NewImmunity)
{
	if (GetOwner()->HasAuthority())
	{
		Immunity = FMath::Clamp(NewImmunity, 0.0f, MaxImmunity);
		OnImmunityChanged.Broadcast(Immunity);
		OnVitalChanged.Broadcast(Immunity);
	}
	else
	{
		// Server RPC call
		ServerModifyImmunity(NewImmunity - Immunity);
	}
}

void UStatsComponent::SetEnergy(float NewEnergy)
{
	if (GetOwner()->HasAuthority())
	{
		Energy = FMath::Clamp(NewEnergy, 0.0f, MaxEnergy);
		OnEnergyChanged.Broadcast(Energy);
		OnVitalChanged.Broadcast(Energy);
	}
	else
	{
		// Server RPC call
		ServerModifyEnergy(NewEnergy - Energy);
	}
}

void UStatsComponent::SetNutrients(float NewNutrients)
{
	if (GetOwner()->HasAuthority())
	{
		Nutrients = FMath::Clamp(NewNutrients, 0.0f, MaxNutrients);
		OnNutrientsChanged.Broadcast(Nutrients);
		OnVitalChanged.Broadcast(Nutrients);
	}
	else
	{
		// Server RPC call
		ServerModifyNutrients(NewNutrients - Nutrients);
	}
}

void UStatsComponent::SetVitamins(float NewVitamins)
{
	if (GetOwner()->HasAuthority())
	{
		Vitamins = FMath::Clamp(NewVitamins, 0.0f, MaxVitamins);
		OnVitaminsChanged.Broadcast(Vitamins);
		OnVitalChanged.Broadcast(Vitamins);
	}
	else
	{
		// Server RPC call
		ServerModifyVitamins(NewVitamins - Vitamins);
	}
}

void UStatsComponent::ModifyHealth(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Health = FMath::Clamp(Health + Delta, 0.0f, MaxHealth);
		OnHealthChanged.Broadcast(Health);
		OnVitalChanged.Broadcast(Health);
	}
	else
	{
		// Server RPC call
		ServerModifyHealth(Delta);
	}
}

void UStatsComponent::ModifyStamina(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Stamina = FMath::Clamp(Stamina + Delta, 0.0f, MaxStamina);
		OnStaminaChanged.Broadcast(Stamina);
		OnVitalChanged.Broadcast(Stamina);
	}
	else
	{
		// Server RPC call
		ServerModifyStamina(Delta);
	}
}

void UStatsComponent::ModifyHunger(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Hunger = FMath::Clamp(Hunger + Delta, 0.0f, MaxHunger);
		OnHungerChanged.Broadcast(Hunger);
		OnVitalChanged.Broadcast(Hunger);
	}
	else
	{
		// Server RPC call
		ServerModifyHunger(Delta);
	}
}

void UStatsComponent::ModifyThirst(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Thirst = FMath::Clamp(Thirst + Delta, 0.0f, MaxThirst);
		OnThirstChanged.Broadcast(Thirst);
		OnVitalChanged.Broadcast(Thirst);
	}
	else
	{
		// Server RPC call
		ServerModifyThirst(Delta);
	}
}

void UStatsComponent::ModifyOxygen(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Oxygen = FMath::Clamp(Oxygen + Delta, 0.0f, MaxOxygen);
		OnOxygenChanged.Broadcast(Oxygen);
		OnVitalChanged.Broadcast(Oxygen);
	}
	else
	{
		// Server RPC call
		ServerModifyOxygen(Delta);
	}
}

void UStatsComponent::ModifyTemperature(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Temperature = FMath::Clamp(Temperature + Delta, 0.0f, MaxTemperature);
		OnTemperatureChanged.Broadcast(Temperature);
		OnVitalChanged.Broadcast(Temperature);
	}
	else
	{
		// Server RPC call
		ServerModifyTemperature(Delta);
	}
}

void UStatsComponent::ModifyPain(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Pain = FMath::Clamp(Pain + Delta, 0.0f, MaxPain);
		OnPainChanged.Broadcast(Pain);
		OnVitalChanged.Broadcast(Pain);
	}
	else
	{
		// Server RPC call
		ServerModifyPain(Delta);
	}
}

void UStatsComponent::ModifyImmunity(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Immunity = FMath::Clamp(Immunity + Delta, 0.0f, MaxImmunity);
		OnImmunityChanged.Broadcast(Immunity);
		OnVitalChanged.Broadcast(Immunity);
	}
	else
	{
		// Server RPC call
		ServerModifyImmunity(Delta);
	}
}

void UStatsComponent::ModifyEnergy(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Energy = FMath::Clamp(Energy + Delta, 0.0f, MaxEnergy);
		OnEnergyChanged.Broadcast(Energy);
		OnVitalChanged.Broadcast(Energy);
	}
	else
	{
		// Server RPC call
		ServerModifyEnergy(Delta);
	}
}

void UStatsComponent::ModifyNutrients(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Nutrients = FMath::Clamp(Nutrients + Delta, 0.0f, MaxNutrients);
		OnNutrientsChanged.Broadcast(Nutrients);
		OnVitalChanged.Broadcast(Nutrients);
	}
	else
	{
		// Server RPC call
		ServerModifyNutrients(Delta);
	}
}

void UStatsComponent::ModifyVitamins(float Delta)
{
	if (GetOwner()->HasAuthority())
	{
		Vitamins = FMath::Clamp(Vitamins + Delta, 0.0f, MaxVitamins);
		OnVitaminsChanged.Broadcast(Vitamins);
		OnVitalChanged.Broadcast(Vitamins);
	}
	else
	{
		// Server RPC call
		ServerModifyVitamins(Delta);
	}
}

void UStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UStatsComponent, Health);
	DOREPLIFETIME(UStatsComponent, MaxHealth);
	DOREPLIFETIME(UStatsComponent, Stamina);
	DOREPLIFETIME(UStatsComponent, MaxStamina);
	DOREPLIFETIME(UStatsComponent, Hunger);
	DOREPLIFETIME(UStatsComponent, MaxHunger);
	DOREPLIFETIME(UStatsComponent, Thirst);
	DOREPLIFETIME(UStatsComponent, MaxThirst);
	DOREPLIFETIME(UStatsComponent, Oxygen);
	DOREPLIFETIME(UStatsComponent, MaxOxygen);
	DOREPLIFETIME(UStatsComponent, Temperature);
	DOREPLIFETIME(UStatsComponent, MaxTemperature);
	DOREPLIFETIME(UStatsComponent, Pain);
	DOREPLIFETIME(UStatsComponent, MaxPain);
	DOREPLIFETIME(UStatsComponent, Immunity);
	DOREPLIFETIME(UStatsComponent, MaxImmunity);
	DOREPLIFETIME(UStatsComponent, Energy);
	DOREPLIFETIME(UStatsComponent, MaxEnergy);
	DOREPLIFETIME(UStatsComponent, Nutrients);
	DOREPLIFETIME(UStatsComponent, MaxNutrients);
	DOREPLIFETIME(UStatsComponent, Vitamins);
	DOREPLIFETIME(UStatsComponent, MaxVitamins);
}

void UStatsComponent::OnRep_Health()
{
	OnHealthChanged.Broadcast(Health);
	OnVitalChanged.Broadcast(Health);
}

void UStatsComponent::OnRep_Stamina()
{
	OnStaminaChanged.Broadcast(Stamina);
	OnVitalChanged.Broadcast(Stamina);
}

void UStatsComponent::OnRep_Hunger()
{
	OnHungerChanged.Broadcast(Hunger);
	OnVitalChanged.Broadcast(Hunger);
}

void UStatsComponent::OnRep_Thirst()
{
	OnThirstChanged.Broadcast(Thirst);
	OnVitalChanged.Broadcast(Thirst);
}

void UStatsComponent::OnRep_Oxygen()
{
	OnOxygenChanged.Broadcast(Oxygen);
	OnVitalChanged.Broadcast(Oxygen);
}

void UStatsComponent::OnRep_Temperature()
{
	OnTemperatureChanged.Broadcast(Temperature);
	OnVitalChanged.Broadcast(Temperature);
}

void UStatsComponent::OnRep_Pain()
{
	OnPainChanged.Broadcast(Pain);
	OnVitalChanged.Broadcast(Pain);
}

void UStatsComponent::OnRep_Immunity()
{
	OnImmunityChanged.Broadcast(Immunity);
	OnVitalChanged.Broadcast(Immunity);
}

void UStatsComponent::OnRep_Energy()
{
	OnEnergyChanged.Broadcast(Energy);
	OnVitalChanged.Broadcast(Energy);
}

void UStatsComponent::OnRep_Nutrients()
{
	OnNutrientsChanged.Broadcast(Nutrients);
	OnVitalChanged.Broadcast(Nutrients);
}

void UStatsComponent::OnRep_Vitamins()
{
	OnVitaminsChanged.Broadcast(Vitamins);
	OnVitalChanged.Broadcast(Vitamins);
}

// Server RPC functions
void UStatsComponent::ServerModifyHealth_Implementation(float Delta)
{
	ModifyHealth(Delta);
}

bool UStatsComponent::ServerModifyHealth_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyStamina_Implementation(float Delta)
{
	ModifyStamina(Delta);
}

bool UStatsComponent::ServerModifyStamina_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyHunger_Implementation(float Delta)
{
	ModifyHunger(Delta);
}

bool UStatsComponent::ServerModifyHunger_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyThirst_Implementation(float Delta)
{
	ModifyThirst(Delta);
}

bool UStatsComponent::ServerModifyThirst_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyOxygen_Implementation(float Delta)
{
	ModifyOxygen(Delta);
}

bool UStatsComponent::ServerModifyOxygen_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyTemperature_Implementation(float Delta)
{
	ModifyTemperature(Delta);
}

bool UStatsComponent::ServerModifyTemperature_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyPain_Implementation(float Delta)
{
	ModifyPain(Delta);
}

bool UStatsComponent::ServerModifyPain_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyImmunity_Implementation(float Delta)
{
	ModifyImmunity(Delta);
}

bool UStatsComponent::ServerModifyImmunity_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyEnergy_Implementation(float Delta)
{
	ModifyEnergy(Delta);
}

bool UStatsComponent::ServerModifyEnergy_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyNutrients_Implementation(float Delta)
{
	ModifyNutrients(Delta);
}

bool UStatsComponent::ServerModifyNutrients_Validate(float Delta)
{
	return true;
}

void UStatsComponent::ServerModifyVitamins_Implementation(float Delta)
{
	ModifyVitamins(Delta);
}

bool UStatsComponent::ServerModifyVitamins_Validate(float Delta)
{
	return true;
}