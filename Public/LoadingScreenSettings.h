// Copyright (c) 2025, Oliver Österlund Stare. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LoadingScreenSettings.generated.h"

/**
 * Settings for the custom loading screen.
 * Allows us to pass parameters and values to the Subsystem from the Editor, since Subsystems can't be derived in Blueprint.
 */
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "Loading Screen"))
class VERTICALSLICE_API ULoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	
	// The widget to load and display as the loading screen.
	UPROPERTY(Config, EditAnywhere,meta = (MetaClass = "/Script/UMG.UserWidget"))
	FSoftClassPath LoadingScreenWidget;

	// The ZOrder used on the widget. Intentionally high to layer above all other content.
	UPROPERTY(Config, EditAnywhere)
	int ZOrder = 10000;

	// How many seconds to keep the screen up after loading is finished.
	UPROPERTY(Config, EditAnywhere, meta = (ForceUnits = s))
	float HoldLoadingScreenAdditionalSecs = 2.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Debugging")
	bool bForceDisplayLoadingScreen = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debugging")
	bool bLogLoadingScreenReason = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debugging")
	bool bShowLoadingScreenAdditionalSecsInEditor = false;
};
