#include "Core/LocalStudio.h"
#include "Core/LocalStudioStyle.h"
#include "Core/LocalStudioCommands.h"
#include "Core/SLocalStudioChatPanel.h"
#include "Core/LocalStudioSettings.h"          
#include "Core/OllamaManager.h"
#include "Core/BuilderRegistry.h"
#include "Core/LocalStudioProjectConfigBuilder.h"

// Modular Builder Infrastructure
#include "Builder/GeneralChatBuilder.h"
#include "Builder/LocalStudioMaterialBuilder.h"
#include "Builder/LocalStudioLandscapeBuilder.h"
#include "Builder/LocalStudioAnimationBuilder.h"
#include "Builder/LocalStudioAudioBuilder.h"
#include "Builder/LocalStudioBlueprintBuilder.h"
#include "Builder/LocalStudioCodeBuilder.h"
#include "Builder/LocalStudioFoliageBuilder.h"
#include "Builder/LocalStudioGameplayAbilityBuilder.h"
#include "Builder/LocalStudioLevelDesignBuilder.h"
#include "Builder/LocalStudioMetaHumanBuilder.h"
#include "Builder/LocalStudioNiagaraBuilder.h"
#include "Builder/LocalStudioStaticMeshBuilder.h"
#include "Builder/LocalStudioUIBuilder.h"
#include "Builder/LocalStudioPCGBuilder.h"

#include "LevelEditor.h"
#include "ToolMenus.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"

#include "Widgets/Docking/SDockTab.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "Core/LocalStudioTaskProfileCustomization.h"
#endif

static const FName LocalStudioTabName("LocalStudio");

#define LOCTEXT_NAMESPACE "FLocalStudioModule"

void FLocalStudioModule::StartupModule()
{
#if WITH_EDITOR
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Use StaticStruct()->GetFName() so UE matches the struct type perfectly!
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FOllamaTaskProfile::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLocalStudioTaskProfileCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomClassLayout(
		ULocalStudioSettings::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&ULocalStudioSettings::MakeInstance)
	);
#endif

	OllamaManager = TStrongObjectPtr<UOllamaManager>(NewObject<UOllamaManager>(GetTransientPackage()));
	OllamaManager->InitializeOllama();

	FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FGeneralChatBuilder>());
	FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioLandscapeBuilder>());
	FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioBlueprintBuilder>());
	FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioMaterialBuilder>());
	FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioCodeBuilder>());



	
	
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioFoliageBuilder>());
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioLevelDesignBuilder>());
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioStaticMeshBuilder>());
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioNiagaraBuilder>());
	
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioAnimationBuilder>());
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioGameplayAbilityBuilder>());
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioAudioBuilder>());
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioUIBuilder>());
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioProjectConfigBuilder>());
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioMetaHumanBuilder>());
	
	//FLocalStudioBuilderRegistry::Get().RegisterBuilder(MakeShared<FLocalStudioPCGBuilder>());

	FLocalStudioStyle::Initialize();
	FLocalStudioStyle::ReloadTextures();
	FLocalStudioCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FLocalStudioCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FLocalStudioModule::PluginButtonClicked),
		FCanExecuteAction()
	);

	RegisterMenus();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LocalStudioTabName, FOnSpawnTab::CreateRaw(this, &FLocalStudioModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FLocalStudioTabTitle", "Local Studio"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FLocalStudioModule::ShutdownModule()
{
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LocalStudioTabName);

	FLocalStudioCommands::Unregister();
	FLocalStudioStyle::Shutdown();

	FLocalStudioBuilderRegistry::Get().UnregisterAll();

#if WITH_EDITOR
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("FOllamaTaskProfile");
		PropertyModule.UnregisterCustomClassLayout("LocalStudioSettings");
	}

	if (GIsEditor && !IsRunningCommandlet())
	{
		EAppReturnType::Type UserChoice = FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("CloseOllamaPrompt", "Would you like to terminate background Ollama processes (ollama.exe / llama-server.exe)?\n\nChoose 'No' if you use Ollama for other applications.")
		);

		if (UserChoice == EAppReturnType::Yes)
		{
			FPlatformProcess::CreateProc(
				TEXT("cmd.exe"),
				TEXT("/c taskkill /f /im ollama.exe"),
				true, true, true, nullptr, 0, nullptr, nullptr
			);

			FPlatformProcess::CreateProc(
				TEXT("cmd.exe"),
				TEXT("/c taskkill /f /im llama-server.exe"),
				true, true, true, nullptr, 0, nullptr, nullptr
			);
		}
	}
#endif

	OllamaManager.Reset();
}

TSharedRef<SDockTab> FLocalStudioModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SLocalStudioChatPanel, OllamaManager.Get())
		];
}

void FLocalStudioModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(LocalStudioTabName);
}

void FLocalStudioModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		if (Menu)
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FLocalStudioCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		if (ToolbarMenu)
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FLocalStudioCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLocalStudioModule, LocalStudio)