#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"

class FUICommandList;
class FToolBarBuilder;
class FMenuBuilder;
class UOllamaManager;

class FLocalStudioModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Bound to command to open the plugin UI tab */
	void PluginButtonClicked();

private:

	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<FUICommandList> PluginCommands;
	TStrongObjectPtr<UOllamaManager> OllamaManager;
};