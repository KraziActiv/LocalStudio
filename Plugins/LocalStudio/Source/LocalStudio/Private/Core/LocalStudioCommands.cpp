#include "Core/LocalStudioCommands.h"

#define LOCTEXT_NAMESPACE "FLocalStudioModule"

void FLocalStudioCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenPluginWindow,
		"LocalStudio",
		"Bring up LocalStudio window",
		EUserInterfaceActionType::Button,
		FInputChord()
	);
}

#undef LOCTEXT_NAMESPACE