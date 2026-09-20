#include "HUD/CharacterHUD.h"
#include "Characters/MasterCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StatsComponent.h"

ACharacterHUD::ACharacterHUD(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    
    BarSize = FVector2D(200.0f, 20.0f);
    BarPosition = FVector2D(20.0f, 20.0f);
    BarSpacing = 30.0f;
    
    BarColor = FLinearColor(0.0f, 0.5f, 1.0f, 1.0f);
    TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void ACharacterHUD::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize the character reference
    if (APlayerController* PC = GetOwningPlayerController())
    {
        if (AMasterCharacter* Character = Cast<AMasterCharacter>(PC->GetPawn()))
        {
            SetMasterCharacter(Character);
        }
    }
}

void ACharacterHUD::DrawHUD()
{
    if (!MasterCharacter || !StatsComponent)
        return;
    
    // Update stats
    UpdateStats();
    
    // Draw progress bars
    const float BarY = BarPosition.Y;
    
    // Health bar
    if (HealthBar)
    {
        HealthBar->SetVisibility(ESlateVisibility::Visible);
        HealthBar->SetPercent(StatsComponent->Health / StatsComponent->MaxHealth);
        HealthBar->SetFillColorAndOpacity(BarColor);
    }
    
    if (HealthText)
    {
        HealthText->SetVisibility(ESlateVisibility::Visible);
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("Health: %.0f/%.0f"), StatsComponent->Health, StatsComponent->MaxHealth)));
        HealthText->SetColorAndOpacity(TextColor);
    }
    
    // Stamina bar
    if (StaminaBar)
    {
        StaminaBar->SetVisibility(ESlateVisibility::Visible);
        StaminaBar->SetPercent(StatsComponent->Stamina / StatsComponent->MaxStamina);
        StaminaBar->SetFillColorAndOpacity(BarColor);
    }
    
    if (StaminaText)
    {
        StaminaText->SetVisibility(ESlateVisibility::Visible);
        StaminaText->SetText(FText::FromString(FString::Printf(TEXT("Stamina: %.0f/%.0f"), StatsComponent->Stamina, StatsComponent->MaxStamina)));
        StaminaText->SetColorAndOpacity(TextColor);
    }
    
    // Hunger bar
    if (HungerBar)
    {
        HungerBar->SetVisibility(ESlateVisibility::Visible);
        HungerBar->SetPercent(StatsComponent->Hunger / StatsComponent->MaxHunger);
        HungerBar->SetFillColorAndOpacity(BarColor);
    }
    
    if (HungerText)
    {
        HungerText->SetVisibility(ESlateVisibility::Visible);
        HungerText->SetText(FText::FromString(FString::Printf(TEXT("Hunger: %.0f/%.0f"), StatsComponent->Hunger, StatsComponent->MaxHunger)));
        HungerText->SetColorAndOpacity(TextColor);
    }
    
    // Thirst bar
    if (ThirstBar)
    {
        ThirstBar->SetVisibility(ESlateVisibility::Visible);
        ThirstBar->SetPercent(StatsComponent->Thirst / StatsComponent->MaxThirst);
        ThirstBar->SetFillColorAndOpacity(BarColor);
    }
    
    if (ThirstText)
    {
        ThirstText->SetVisibility(ESlateVisibility::Visible);
        ThirstText->SetText(FText::FromString(FString::Printf(TEXT("Thirst: %.0f/%.0f"), StatsComponent->Thirst, StatsComponent->MaxThirst)));
        ThirstText->SetColorAndOpacity(TextColor);
    }
}

void ACharacterHUD::UpdateStats()
{
    if (!MasterCharacter || !StatsComponent)
        return;
    
    // Stats are read directly from the component
}

void ACharacterHUD::SetMasterCharacter(AMasterCharacter* InCharacter)
{
    MasterCharacter = InCharacter;
    
    if (MasterCharacter)
    {
        StatsComponent = MasterCharacter->StatsComponent;
        
        if (StatsComponent)
        {
            // Bind to stat change events
            StatsComponent->OnHealthChanged.AddDynamic(this, &ACharacterHUD::OnHealthChanged);
            StatsComponent->OnStaminaChanged.AddDynamic(this, &ACharacterHUD::OnStaminaChanged);
            StatsComponent->OnHungerChanged.AddDynamic(this, &ACharacterHUD::OnHungerChanged);
            StatsComponent->OnThirstChanged.AddDynamic(this, &ACharacterHUD::OnThirstChanged);
        }
    }
}

void ACharacterHUD::OnHealthChanged(float NewValue)
{
    // Trigger HUD update when health changes
    if (this && GetWorld())
    {
        DrawHUD();
    }
}

void ACharacterHUD::OnStaminaChanged(float NewValue)
{
    // Trigger HUD update when stamina changes
    if (this && GetWorld())
    {
        DrawHUD();
    }
}

void ACharacterHUD::OnHungerChanged(float NewValue)
{
    // Trigger HUD update when hunger changes
    if (this && GetWorld())
    {
        DrawHUD();
    }
}

void ACharacterHUD::OnThirstChanged(float NewValue)
{
    // Trigger HUD update when thirst changes
    if (this && GetWorld())
    {
        DrawHUD();
    }
}