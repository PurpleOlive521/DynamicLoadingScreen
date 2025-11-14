// Copyright (c) 2025, Oliver Österlund Stare. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Tickable.h"

#include "LoadingScreenSubsystem.generated.h"

class SWidget;
class UObject;
class UWorld;
struct FFrame; 
struct FWorldContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHoldTimeTriggeredSignature, float, HoldTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVisibilityChangedSignature, bool, Visiblity);


/*
* Handles displaying a loading screen during level transitions, or explicitly when requested by game code.
* Inspired by Lyra's LoadingScreenManager.
*/
UCLASS()
class VERTICALSLICE_API ULoadingScreenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	
	// --- Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// --- End USubsystem Interface

	// --- Begin FTickableObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual bool IsTickableWhenPaused() const override;
	// --- End FTickableObject Interface

	// Gets the display state. Returns true if the loading screen is displayed, false if not.
	UFUNCTION(BlueprintCallable)
	bool IsLoadingScreenDisplayed() const;

	// Forces the loading screen to be displayed or hidden. Intended for use by game logic to predisplay the loading screen, or keep it on for a bit longer.
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Force Loading Screen Visibility"))
	void ForceDisplayStateByGameLogic(bool Visibility, FString Reason);

	// Returns true if the loading screen is currently waiting for HoldLoadingScreenAdditionalSecs to pass.
	UFUNCTION(BlueprintCallable)
	bool IsWaitingForAdditionalTime() const;

	// Calculates the HoldLoadingScreenAdditionalSecs remaining before the loading screen is removed. 
	// Will be undefined if not active!
	UFUNCTION(BlueprintCallable)
	float GetAdditionalTimeRemaining() const;


private:
	// CoreUObject hookups
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* World);

	// Does multiple checks to determine if a loading screen is needed. 
	bool CheckForDisplayReason();

	void UpdateLoadingScreen();

	// Checks if the widget should be removed because of loading being done.
	bool ShouldShowLoadingScreen();

	// Displays the loading screen and sets up the widget containing the dynamic content.
	void ShowLoadingScreen();

	// Hides the loading screeen if displayed by destroying it.
	void HideLoadingScreen();

	// Removes the widget from the viewport.
	void RemoveWidget();

	// Does some performance optimisations during the loading screen, such as stopping geometry from being drawn on screen.
	void ChangePerformanceSettings(bool bEnabingLoadingScreen);

	// The displayed widget if any. Used to update the widget manually. Do not confuse with the class that the widget is created from!
	TSharedPtr<SWidget> LoadingScreenWidget;

	bool bIsDisplayingLoadingScreen;

	// The reason for the latest change in the loading screens visibility state. Used for debugging purposes only!
	FString LoadingScreenStateReason;

	double LoadingScreenLastDismissedTimestamp = -1.0;

	bool bIsDisplayedByGameLogic = false;

	// Set by user when calling ForceDisplayStateByGameLogic
	FString UserSpecifiedLoadingScreenReason;

public: 

	// Called when the loading screen is waiting for HoldLoadingScreenAdditionalSecs to pass. Passes said value.
	UPROPERTY(BlueprintAssignable)
	FOnHoldTimeTriggeredSignature OnHoldTimeTriggeredDelegate;

	// Called when the loading screens visibility is changed, passing the new state.
	UPROPERTY(BlueprintAssignable)
	FOnVisibilityChangedSignature OnVisibilityChangedDelegate;
};
