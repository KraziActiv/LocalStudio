#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CharacterHUD.generated.h"

class AMasterCharacter;
struct FProgressBar;
struct FTextBlock;
class UStatsComponent;

class UProgressBar;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LOCALSTUDIOPROJECT_API ACharacterHUD : public AHUD
{
    GENERATED_BODY()

public:
    ACharacterHUD(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    
    virtual void DrawHUD() override;
    
    void UpdateStats();
    
    void SetMasterCharacter(AMasterCharacter* InCharacter);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    AMasterCharacter* MasterCharacter;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UStatsComponent* StatsComponent;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UProgressBar* HealthBar;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UProgressBar* StaminaBar;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UProgressBar* HungerBar;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UProgressBar* ThirstBar;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UTextBlock* HealthText;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UTextBlock* StaminaText;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UTextBlock* HungerText;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
    UTextBlock* ThirstText;
    
    FVector2D BarSize;
    FVector2D BarPosition;
    
    float BarSpacing;
    
    FLinearColor BarColor;
    FLinearColor TextColor;
    
    UFUNCTION()
    void OnHealthChanged(float NewValue);
    
    UFUNCTION()
    void OnStaminaChanged(float NewValue);
    
    UFUNCTION()
    void OnHungerChanged(float NewValue);
    
    UFUNCTION()
    void OnThirstChanged(float NewValue);
};