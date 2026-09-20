#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class LOCALSTUDIO_API FLocalStudioStyle
{
public:

	static void Initialize();

	static void Shutdown();

	/** Reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set for LocalStudio */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef<class FSlateStyleSet> Create();

private:

	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};