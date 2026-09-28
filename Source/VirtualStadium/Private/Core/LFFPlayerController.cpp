#include "Core/LFFPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Journey/LFFJourneySubsystem.h"
#include "UI/LFFHUDWidget.h"
#include "VirtualStadium.h"

ALFFPlayerController::ALFFPlayerController()
{
    // The journey subsystem owns the view target; don't let the engine switch it back to a pawn.
    bAutoManageActiveCameraTarget = false;
    bShowMouseCursor = true;

    HUDWidgetClass = TSoftClassPtr<ULFFHUDWidget>(FSoftObjectPath(TEXT("/Game/LFF/UI/WBP_LFF_HUD.WBP_LFF_HUD_C")));
}

void ALFFPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    BuildInputObjects();
    if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        InputSubsystem->AddMappingContext(JourneyContext, 0);
    }

    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);

    if (UClass* WidgetClass = HUDWidgetClass.LoadSynchronous())
    {
        HUDWidget = CreateWidget<ULFFHUDWidget>(this, WidgetClass);
        if (HUDWidget != nullptr)
        {
            HUDWidget->AddToViewport();
        }
    }

    if (HUDWidget == nullptr)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("HUD widget missing at %s. The disclaimer cannot be shown; do not ship this build."), *HUDWidgetClass.ToString());
    }
}

void ALFFPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    BuildInputObjects();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (EnhancedInput == nullptr)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("InputComponent is not an EnhancedInputComponent. Check DefaultInput.ini."));
        return;
    }

    // Started fires once per press, so holding a key never repeats a step.
    EnhancedInput->BindAction(NextAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleNext);
    EnhancedInput->BindAction(BackAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleBack);
    EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandlePause);
    EnhancedInput->BindAction(SettingsAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleSettings);
    EnhancedInput->BindAction(RestartAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleRestart);
    EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleConfirm);
    EnhancedInput->BindAction(CancelAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleCancel);
    EnhancedInput->BindAction(ToggleReducedMotionAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleReducedMotion);
    EnhancedInput->BindAction(ToggleSoundAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleSound);
    EnhancedInput->BindAction(ToggleCalmModeAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleCalmMode);
    EnhancedInput->BindAction(ToggleLargeTextAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleLargeText);
}

UInputAction* ALFFPlayerController::MakeAction(const TCHAR* Name)
{
    // UInputAction defaults to a Boolean (digital) value, which is what every journey action needs.
    return NewObject<UInputAction>(this, FName(Name));
}

void ALFFPlayerController::BuildInputObjects()
{
    if (JourneyContext != nullptr)
    {
        return;
    }

    NextAction = MakeAction(TEXT("IA_LFF_Next"));
    BackAction = MakeAction(TEXT("IA_LFF_Back"));
    PauseAction = MakeAction(TEXT("IA_LFF_Pause"));
    SettingsAction = MakeAction(TEXT("IA_LFF_Settings"));
    RestartAction = MakeAction(TEXT("IA_LFF_Restart"));
    ConfirmAction = MakeAction(TEXT("IA_LFF_Confirm"));
    CancelAction = MakeAction(TEXT("IA_LFF_Cancel"));
    ToggleReducedMotionAction = MakeAction(TEXT("IA_LFF_ToggleReducedMotion"));
    ToggleSoundAction = MakeAction(TEXT("IA_LFF_ToggleSound"));
    ToggleCalmModeAction = MakeAction(TEXT("IA_LFF_ToggleCalmMode"));
    ToggleLargeTextAction = MakeAction(TEXT("IA_LFF_ToggleLargeText"));

    JourneyContext = NewObject<UInputMappingContext>(this, TEXT("IMC_LFF_Journey"));

    // Keep this table in sync with Docs/JOURNEY_SCHEMA.md "Input".
    JourneyContext->MapKey(NextAction, EKeys::Right);
    JourneyContext->MapKey(NextAction, EKeys::SpaceBar);
    JourneyContext->MapKey(NextAction, EKeys::PageDown);
    JourneyContext->MapKey(NextAction, EKeys::Gamepad_DPad_Right);
    JourneyContext->MapKey(NextAction, EKeys::Gamepad_RightShoulder);

    JourneyContext->MapKey(BackAction, EKeys::Left);
    JourneyContext->MapKey(BackAction, EKeys::BackSpace);
    JourneyContext->MapKey(BackAction, EKeys::PageUp);
    JourneyContext->MapKey(BackAction, EKeys::Gamepad_DPad_Left);
    JourneyContext->MapKey(BackAction, EKeys::Gamepad_LeftShoulder);

    JourneyContext->MapKey(PauseAction, EKeys::P);
    JourneyContext->MapKey(PauseAction, EKeys::Gamepad_Special_Right);

    JourneyContext->MapKey(SettingsAction, EKeys::S);
    JourneyContext->MapKey(SettingsAction, EKeys::Gamepad_Special_Left);

    JourneyContext->MapKey(RestartAction, EKeys::R);
    JourneyContext->MapKey(RestartAction, EKeys::Gamepad_FaceButton_Top);

    // Confirm answers a question on screen; with nothing to answer it acts as Next.
    JourneyContext->MapKey(ConfirmAction, EKeys::Enter);
    JourneyContext->MapKey(ConfirmAction, EKeys::Gamepad_FaceButton_Bottom);

    JourneyContext->MapKey(CancelAction, EKeys::Escape);
    JourneyContext->MapKey(CancelAction, EKeys::Gamepad_FaceButton_Right);

    JourneyContext->MapKey(ToggleReducedMotionAction, EKeys::One);
    JourneyContext->MapKey(ToggleSoundAction, EKeys::Two);
    JourneyContext->MapKey(ToggleCalmModeAction, EKeys::Three);
    JourneyContext->MapKey(ToggleLargeTextAction, EKeys::Four);
}

ULFFJourneySubsystem* ALFFPlayerController::GetJourney() const
{
    UWorld* World = GetWorld();
    return World != nullptr ? World->GetSubsystem<ULFFJourneySubsystem>() : nullptr;
}

void ALFFPlayerController::HandleNext()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->Next();
    }
}

void ALFFPlayerController::HandleBack()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->Back();
    }
}

void ALFFPlayerController::HandlePause()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->TogglePause();
    }
}

void ALFFPlayerController::HandleSettings()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleSettings();
    }
}

void ALFFPlayerController::HandleRestart()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->RequestRestart();
    }
}

void ALFFPlayerController::HandleConfirm()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->Confirm();
    }
}

void ALFFPlayerController::HandleCancel()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->Cancel();
    }
}

void ALFFPlayerController::HandleToggleReducedMotion()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleReducedMotion();
    }
}

void ALFFPlayerController::HandleToggleSound()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleSound();
    }
}

void ALFFPlayerController::HandleToggleCalmMode()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleCalmMode();
    }
}

void ALFFPlayerController::HandleToggleLargeText()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleLargeText();
    }
}
