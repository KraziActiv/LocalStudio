#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "LocalStudioStyle.h"

class LOCALSTUDIO_API FLocalStudioCommands : public TCommands<FLocalStudioCommands>
{
public:

	FLocalStudioCommands()
		: TCommands<FLocalStudioCommands>(
			TEXT("LocalStudio"),
			NSLOCTEXT("Contexts", "LocalStudio", "LocalStudio Plugin"),
			NAME_None,
			FLocalStudioStyle::GetStyleSetName()
		)
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> OpenPluginWindow;
};