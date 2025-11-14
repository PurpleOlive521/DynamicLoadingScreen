// Copyright (c) 2025, Oliver Österlund Stare. All rights reserved.


#include "LoadingScreenSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

#include "LoadingScreenSettings.h"

#include "Framework/Application/SlateApplication.h" // For prompting slate tick

#include "Widgets/Images/SThrobber.h" // Fallback widget
#include "Blueprint/UserWidget.h"

#include "DevCommons.h"

// USubsystem Begin
void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

    const UGameInstance* LocalGameInstance = GetGameInstance();

    if (!LocalGameInstance)
    {
        UE_LOG(VSLog, Error, TEXT("Could not get GameInstance on Init."));
    }
}

void ULoadingScreenSubsystem::Deinitialize()
{
    RemoveWidget();

    FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
}

bool ULoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Prevents servers or other instances from initializing this subsystem
    const UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
    const bool bIsServerWorld = GameInstance->IsDedicatedServerInstance();
    return !bIsServerWorld;
}
// USubsystem End


// FTickableObject Begin

void ULoadingScreenSubsystem::Tick(float DeltaTime)
{
    UpdateLoadingScreen();
}

ETickableTickType ULoadingScreenSubsystem::GetTickableTickType() const
{
    // Set to Conditional to ensure that CDO is not marked for ticking
    return ETickableTickType::Conditional;
}

bool ULoadingScreenSubsystem::IsTickable() const
{
    // No ticking for CDO
    return !HasAnyFlags(RF_ClassDefaultObject);
}

TStatId ULoadingScreenSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(ULoadingScreenManager, STATGROUP_Tickables);
}

UWorld* ULoadingScreenSubsystem::GetTickableGameObjectWorld() const
{
    return GetGameInstance()->GetWorld();
}

bool ULoadingScreenSubsystem::IsTickableWhenPaused() const
{
    // We want the subsystem to check for loading screen reasons even when in a pause screens or similar situations
    return true;
}

//FTickableObject End


bool ULoadingScreenSubsystem::IsLoadingScreenDisplayed() const
{
    return bIsDisplayingLoadingScreen;
}

void ULoadingScreenSubsystem::ForceDisplayStateByGameLogic(bool Visibility, FString Reason)
{
    UserSpecifiedLoadingScreenReason = Reason;
	bIsDisplayedByGameLogic = Visibility;
}

bool ULoadingScreenSubsystem::IsWaitingForAdditionalTime() const
{
    // Loading screen isnt even displayed
    if (bIsDisplayingLoadingScreen == false)
    {
        return false;
    }

    // No additional time specified
    const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();
    if (Settings->HoldLoadingScreenAdditionalSecs <= 0.0)
    {
        return false;
    }
   
    // Timestamp is set and valid, we can assume we are waiting for the additional time
    if (LoadingScreenLastDismissedTimestamp > 0.0)
    {
        return true;
    }

    return false;
}

float ULoadingScreenSubsystem::GetAdditionalTimeRemaining() const
{
    const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

    const double CurrentTime = FPlatformTime::Seconds();

    const double TimeSinceScreenDismissed = CurrentTime - LoadingScreenLastDismissedTimestamp;

    return Settings->HoldLoadingScreenAdditionalSecs - TimeSinceScreenDismissed;
}

void ULoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
    // Immediately update the loading screen once to initialize logic.
    if (GEngine->IsInitialized())
    {
        UpdateLoadingScreen();
    }
}

void ULoadingScreenSubsystem::HandlePostLoadMap(UWorld* World)
{
}

bool ULoadingScreenSubsystem::CheckForDisplayReason()
{
    const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

    // Forced in settings. Show loading screen!
    if (Settings->bForceDisplayLoadingScreen == true)
    {
        LoadingScreenStateReason = FString(TEXT("ForceDisplayLoadingScreen in settings is true"));
        return true;
    }

    const UGameInstance* LocalGameInstance = GetGameInstance();

    // No world context, probably no level. Show loading screen!
    const FWorldContext* Context = LocalGameInstance->GetWorldContext();
    if (!Context)
    {
        LoadingScreenStateReason = FString(TEXT("The game instance has a null WorldContext"));
        return true;
    }

	// No world, show loading screen!
    UWorld* World = Context->World();
    if (World == nullptr)
    {
        LoadingScreenStateReason = FString(TEXT("We have no world is null, FWorldContext's World()"));
        return true;
    }

	// World isnt ready, show loading screen!
    if (!World->HasBegunPlay())
    {
        LoadingScreenStateReason = FString(TEXT("World hasn't begun play"));
        return true;
    }

    // Game logic has requested the loading screen, show it!
    if (bIsDisplayedByGameLogic == true)
    {
        if (UserSpecifiedLoadingScreenReason.IsEmpty())
        {
		    LoadingScreenStateReason = FString(TEXT("Reason not specified in ForceDisplayStateByGameLogic. Assumed gameplay logic."));
        }
        else
        {
            LoadingScreenStateReason = UserSpecifiedLoadingScreenReason;
        }

		return true;
    }

    // No checks returned true, no reason to show
    LoadingScreenStateReason = FString(TEXT("No reason to display."));
    return false;
}

void ULoadingScreenSubsystem::UpdateLoadingScreen()
{
    if (ShouldShowLoadingScreen())
    {
		ShowLoadingScreen();
	}
    else
    {
        HideLoadingScreen();
    }

    const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();
    
    if (Settings->bLogLoadingScreenReason == true)
    {
        UE_LOG(VSLog, Log, TEXT("Loading screen display status: %d. Reason: %s"), bIsDisplayingLoadingScreen ? 1 : 0, *LoadingScreenStateReason);
    }
}

bool ULoadingScreenSubsystem::ShouldShowLoadingScreen()
{
    bool bNeedToShowLoadingScreen = CheckForDisplayReason();

    const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

    // Loading screen can be forced on bc of minimum on-times or other requirements.
    bool bForcedToShowLoadingScreen = false;
    if (bNeedToShowLoadingScreen)
    {
        // Still need to show it for other reasons, dont update
        LoadingScreenLastDismissedTimestamp = -1.0;
    }
    else
    {
        const double CurrentTime = FPlatformTime::Seconds();
        double HoldLoadingScreenTime = Settings->HoldLoadingScreenAdditionalSecs;

        // If in editor and with waiting turned off, skip the additional time entirely
        if (GIsEditor && Settings->bShowLoadingScreenAdditionalSecsInEditor == false)
        {
            HoldLoadingScreenTime = 0.0;
        }

        // Setup the timestamp the first time this is hit
        if (LoadingScreenLastDismissedTimestamp < 0.0)
        {
            LoadingScreenLastDismissedTimestamp = CurrentTime;
            OnHoldTimeTriggeredDelegate.Broadcast(HoldLoadingScreenTime);
        }
        const double TimeSinceScreenDismissed = CurrentTime - LoadingScreenLastDismissedTimestamp;

        // Hold for an extra X seconds, to cover up geometry loading
        if ((HoldLoadingScreenTime > 0.0) && (TimeSinceScreenDismissed < HoldLoadingScreenTime))
        {
            // Make sure we're rendering the world at this point, so that textures will actually stream in
            UGameViewportClient* GameViewportClient = GetGameInstance()->GetGameViewportClient();
            GameViewportClient->bDisableWorldRendering = false;

            LoadingScreenStateReason = FString::Printf(TEXT("Keeping loading screen up for an additional %.2f seconds to allow texture streaming"), Settings->HoldLoadingScreenAdditionalSecs);
            bForcedToShowLoadingScreen = true;
        }
    }

    return bNeedToShowLoadingScreen || bForcedToShowLoadingScreen;
}

void ULoadingScreenSubsystem::ShowLoadingScreen()
{
    // Already onscreen
    if (bIsDisplayingLoadingScreen)
    {
        return;
    }
    
    bIsDisplayingLoadingScreen = true;

    OnVisibilityChangedDelegate.Broadcast(bIsDisplayingLoadingScreen);

    UGameInstance* LocalGameInstance = GetGameInstance();

    const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

    // Create and show widget
    TSubclassOf<UUserWidget> LoadingScreenWidgetClass = Settings->LoadingScreenWidget.TryLoadClass<UUserWidget>();
    if (UUserWidget* UserWidget = UUserWidget::CreateWidgetInstance(*LocalGameInstance, LoadingScreenWidgetClass, NAME_None))
    {
        LoadingScreenWidget = UserWidget->TakeWidget();
    }
    else
    {
        UE_LOG(VSLog, Error, TEXT("Failed to load the loading screen widget '%s', falling back to placeholder."), *Settings->LoadingScreenWidget.ToString());
        LoadingScreenWidget = SNew(SThrobber);
    }

    // Add to the viewport at a high ZOrder to make sure it is on top of most things
    UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
    GameViewportClient->AddViewportWidgetContent(LoadingScreenWidget.ToSharedRef(), Settings->ZOrder);

    ChangePerformanceSettings(true);

    if (!GIsEditor)
    {
        // Tick Slate to make sure the loading screen is displayed immediately
        FSlateApplication::Get().Tick();
    }
}

void ULoadingScreenSubsystem::HideLoadingScreen()
{
    // Already offscreen
    if (!bIsDisplayingLoadingScreen)
    {
        return;
    }

    RemoveWidget();

    ChangePerformanceSettings(false);

    bIsDisplayingLoadingScreen = false;

    OnVisibilityChangedDelegate.Broadcast(bIsDisplayingLoadingScreen);
}

void ULoadingScreenSubsystem::RemoveWidget()
{
    // Gets the widget if the sharedptr is valid, before resetting it and destroying the object
    UGameInstance* LocalGameInstance = GetGameInstance();
    if (LoadingScreenWidget.IsValid())
    {
        if (UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient())
        {
            GameViewportClient->RemoveViewportWidgetContent(LoadingScreenWidget.ToSharedRef());
        }
        LoadingScreenWidget.Reset();
    }
}

void ULoadingScreenSubsystem::ChangePerformanceSettings(bool bEnabingLoadingScreen)
{
    UGameInstance* LocalGameInstance = GetGameInstance();
    UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();

    // Could later look into changing the ShaderPipelineCaching to toggle between Fast and Background for even quicker loading.

    // Don't bother drawing the 3D world while we're loading
    GameViewportClient->bDisableWorldRendering = bEnabingLoadingScreen;

    // Make sure to prioritize streaming in levels if the loading screen is up
    if (UWorld* ViewportWorld = GameViewportClient->GetWorld())
    {
        if (AWorldSettings* WorldSettings = ViewportWorld->GetWorldSettings(false, false))
        {
            WorldSettings->bHighPriorityLoadingLocal = bEnabingLoadingScreen;
        }
    }
}
